/*
 * vim: set ft=rust:
 * vim: set ft=reason:
 *
 * Copyright (c) 2009 - 2013 Monoidics ltd.
 * Copyright (c) 2013 - present Facebook, Inc.
 * All rights reserved.
 *
 * This source code is licensed under the BSD style license found in the
 * LICENSE file in the root directory of this source tree. An additional grant
 * of patent rights can be found in the PATENTS file in the same directory.
 */
open! Utils;

let module L = Logging;

let module F = Format;

/* =============== START of module Node =============== */
let module Node = {
  type id = int;
  type nodekind =
    | Start_node Procname.t
    | Exit_node Procname.t
    | Stmt_node string
    | Join_node
    | Prune_node bool Sil.if_kind string /** (true/false branch, if_kind, comment) */
    | Skip_node string;

  /** a node */
  type t = {
    /** unique id of the node */
    id: id,
    /** distance to the exit node */
    mutable dist_exit: option int,
    /** exception nodes in the cfg */
    mutable exn: list t,
    /** instructions for symbolic execution */
    mutable instrs: list Sil.instr,
    /** kind of node */
    kind: nodekind,
    /** location in the source code */
    loc: Location.t,
    /** predecessor nodes in the cfg */
    mutable preds: list t,
    /** name of the procedure the node belongs to */
    pname: option Procname.t,
    /** successor nodes in the cfg */
    mutable succs: list t
  };
  let exn_handler_kind = Stmt_node "exception handler";
  let exn_sink_kind = Stmt_node "exceptions sink";
  let throw_kind = Stmt_node "throw";
  let dummy () => {
    id: 0,
    dist_exit: None,
    instrs: [],
    kind: Skip_node "dummy",
    loc: Location.dummy,
    pname: None,
    succs: [],
    preds: [],
    exn: []
  };
  let compare node1 node2 => int_compare node1.id node2.id;
  let hash node => Hashtbl.hash node.id;
  let equal node1 node2 => compare node1 node2 == 0;

  /** Get the unique id of the node */
  let get_id node => node.id;

  /** compare node ids */
  let id_compare = int_compare;
  let get_succs node => node.succs;
  type node = t;
  let module NodeSet = Set.Make {
    type t = node;
    let compare = compare;
  };
  let module IdMap = Map.Make {
    type t = id;
    let compare = id_compare;
  };
  let get_sliced_succs node f => {
    let visited = ref NodeSet.empty;
    let rec slice_nodes nodes :NodeSet.t => {
      let do_node acc n => {
        visited := NodeSet.add n !visited;
        if (f n) {
          NodeSet.singleton n
        } else {
          NodeSet.union
            acc (slice_nodes (IList.filter (fun s => not (NodeSet.mem s !visited)) n.succs))
        }
      };
      IList.fold_left do_node NodeSet.empty nodes
    };
    NodeSet.elements (slice_nodes node.succs)
  };
  let get_sliced_preds node f => {
    let visited = ref NodeSet.empty;
    let rec slice_nodes nodes :NodeSet.t => {
      let do_node acc n => {
        visited := NodeSet.add n !visited;
        if (f n) {
          NodeSet.singleton n
        } else {
          NodeSet.union
            acc (slice_nodes (IList.filter (fun s => not (NodeSet.mem s !visited)) n.preds))
        }
      };
      IList.fold_left do_node NodeSet.empty nodes
    };
    NodeSet.elements (slice_nodes node.preds)
  };
  let get_exn node => node.exn;

  /** Get the name of the procedure the node belongs to */
  let get_proc_name node =>
    switch node.pname {
    | None =>
      L.out "get_proc_name: at node %d@\n" node.id;
      assert false
    | Some pname => pname
    };

  /** Get the predecessors of the node */
  let get_preds node => node.preds;

  /** Generates a list of nodes starting at a given node
      and recursively adding the results of the generator */
  let get_generated_slope start_node generator => {
    let visited = ref NodeSet.empty;
    let rec nodes n => {
      visited := NodeSet.add n !visited;
      let succs = IList.filter (fun n => not (NodeSet.mem n !visited)) (generator n);
      switch (IList.length succs) {
      | 1 => [n, ...nodes (IList.hd succs)]
      | _ => [n]
      }
    };
    nodes start_node
  };

  /** Get the node kind */
  let get_kind node => node.kind;

  /** Comparison for node kind */
  let kind_compare k1 k2 =>
    switch (k1, k2) {
    | (Start_node pn1, Start_node pn2) => Procname.compare pn1 pn2
    | (Start_node _, _) => (-1)
    | (_, Start_node _) => 1
    | (Exit_node pn1, Exit_node pn2) => Procname.compare pn1 pn2
    | (Exit_node _, _) => (-1)
    | (_, Exit_node _) => 1
    | (Stmt_node s1, Stmt_node s2) => string_compare s1 s2
    | (Stmt_node _, _) => (-1)
    | (_, Stmt_node _) => 1
    | (Join_node, Join_node) => 0
    | (Join_node, _) => (-1)
    | (_, Join_node) => 1
    | (Prune_node is_true_branch1 if_kind1 descr1, Prune_node is_true_branch2 if_kind2 descr2) =>
      let n = bool_compare is_true_branch1 is_true_branch2;
      if (n != 0) {
        n
      } else {
        let n = Pervasives.compare if_kind1 if_kind2;
        if (n != 0) {
          n
        } else {
          string_compare descr1 descr2
        }
      }
    | (Prune_node _, _) => (-1)
    | (_, Prune_node _) => 1
    | (Skip_node s1, Skip_node s2) => string_compare s1 s2
    };

  /** Get the instructions to be executed */
  let get_instrs node => node.instrs;

  /** Get the list of callee procnames from the node */
  let get_callees node => {
    let collect callees instr =>
      switch instr {
      | Sil.Call _ exp _ _ _ =>
        switch exp {
        | Exp.Const (Const.Cfun procname) => [procname, ...callees]
        | _ => callees
        }
      | _ => callees
      };
    IList.fold_left collect [] (get_instrs node)
  };

  /** Get the location of the node */
  let get_loc n => n.loc;

  /** Get the source location of the last instruction in the node */
  let get_last_loc n =>
    switch (IList.rev (get_instrs n)) {
    | [instr, ..._] => Sil.instr_get_loc instr
    | [] => n.loc
    };
  let pp_id f id => F.fprintf f "%d" id;
  let pp f node => pp_id f (get_id node);
  let get_distance_to_exit node => node.dist_exit;

  /** Append the instructions to the list of instructions to execute */
  let append_instrs node instrs => node.instrs = node.instrs @ instrs;

  /** Add the instructions at the beginning of the list of instructions to execute */
  let prepend_instrs node instrs => node.instrs = instrs @ node.instrs;

  /** Replace the instructions to be executed. */
  let replace_instrs node instrs => node.instrs = instrs;

  /** Add declarations for local variables and return variable to the node */
  let add_locals_ret_declaration node (proc_attributes: ProcAttributes.t) locals => {
    let loc = get_loc node;
    let pname = proc_attributes.proc_name;
    let ret_var = {
      let ret_type = proc_attributes.ret_type;
      (Pvar.get_ret_pvar pname, ret_type)
    };
    let construct_decl (x, typ) => (Pvar.mk x pname, typ);
    let ptl = [ret_var, ...IList.map construct_decl locals];
    let instr = Sil.Declare_locals ptl loc;
    prepend_instrs node [instr]
  };

  /** Print extended instructions for the node,
      highlighting the given subinstruction if present */
  let pp_instrs pe0 sub_instrs::sub_instrs instro fmt node => {
    let pe =
      switch instro {
      | None => pe0
      | Some instr => pe_extend_colormap pe0 (Obj.repr instr) Red
      };
    let instrs = get_instrs node;
    let pp_loc fmt () => F.fprintf fmt " %a " Location.pp (get_loc node);
    let print_sub_instrs () => F.fprintf fmt "%a" (Sil.pp_instr_list pe) instrs;
    switch (get_kind node) {
    | Stmt_node s =>
      if sub_instrs {
        print_sub_instrs ()
      } else {
        F.fprintf fmt "statements (%s) %a" s pp_loc ()
      }
    | Prune_node _ _ descr =>
      if sub_instrs {
        print_sub_instrs ()
      } else {
        F.fprintf fmt "assume %s %a" descr pp_loc ()
      }
    | Exit_node _ =>
      if sub_instrs {
        print_sub_instrs ()
      } else {
        F.fprintf fmt "exit %a" pp_loc ()
      }
    | Skip_node s =>
      if sub_instrs {
        print_sub_instrs ()
      } else {
        F.fprintf fmt "skip (%s) %a" s pp_loc ()
      }
    | Start_node _ =>
      if sub_instrs {
        print_sub_instrs ()
      } else {
        F.fprintf fmt "start %a" pp_loc ()
      }
    | Join_node =>
      if sub_instrs {
        print_sub_instrs ()
      } else {
        F.fprintf fmt "join %a" pp_loc ()
      }
    }
  };

  /** Dump extended instructions for the node */
  let d_instrs sub_instrs::(sub_instrs: bool) (curr_instr: option Sil.instr) (node: t) => L.add_print_action (
    L.PTnode_instrs,
    Obj.repr (sub_instrs, curr_instr, node)
  );

  /** Return a description of the cfg node */
  let get_description pe node => {
    let str =
      switch (get_kind node) {
      | Stmt_node _ => "Instructions"
      | Prune_node _ _ descr => "Conditional" ^ " " ^ descr
      | Exit_node _ => "Exit"
      | Skip_node _ => "Skip"
      | Start_node _ => "Start"
      | Join_node => "Join"
      };
    let pp fmt () => F.fprintf fmt "%s\n%a@?" str (pp_instrs pe None sub_instrs::true) node;
    pp_to_string pp ()
  };
};

/* =============== END of module Node =============== */

/** Map over nodes */
let module NodeMap = Map.Make Node;


/** Hash table with nodes as keys. */
let module NodeHash = Hashtbl.Make Node;


/** Set of nodes. */
let module NodeSet = Node.NodeSet;


/** Map with node id keys. */
let module IdMap = Node.IdMap;


/** procedure description */
type t = {
  attributes: ProcAttributes.t, /** attributes of the procedure */
  mutable nodes: list Node.t, /** list of nodes of this procedure */
  mutable nodes_num: int, /** number of nodes */
  mutable start_node: Node.t, /** start node of this procedure */
  mutable exit_node: Node.t /** exit node of ths procedure */
};


/** Only call from Cfg */
let from_proc_attributes called_from_cfg::called_from_cfg attributes => {
  if (not called_from_cfg) {
    assert false
  };
  {attributes, nodes: [], nodes_num: 0, start_node: Node.dummy (), exit_node: Node.dummy ()}
};


/** Compute the distance of each node to the exit node, if not computed already */
let compute_distance_to_exit_node pdesc => {
  let exit_node = pdesc.exit_node;
  let rec mark_distance dist nodes => {
    let next_nodes = ref [];
    let do_node (node: Node.t) =>
      switch node.dist_exit {
      | Some _ => ()
      | None =>
        node.dist_exit = Some dist;
        next_nodes := node.preds @ !next_nodes
      };
    IList.iter do_node nodes;
    if (!next_nodes !== []) {
      mark_distance (dist + 1) !next_nodes
    }
  };
  mark_distance 0 [exit_node]
};


/** check or indicate if we have performed preanalysis on the CFG */
let did_preanalysis pdesc => pdesc.attributes.did_preanalysis;

let signal_did_preanalysis pdesc => pdesc.attributes.did_preanalysis = true;

let get_attributes pdesc => pdesc.attributes;

let get_err_log pdesc => pdesc.attributes.err_log;

let get_exit_node pdesc => pdesc.exit_node;


/** Get flags for the proc desc */
let get_flags pdesc => pdesc.attributes.proc_flags;


/** Return name and type of formal parameters */
let get_formals pdesc => pdesc.attributes.formals;

let get_loc pdesc => pdesc.attributes.loc;


/** Return name and type of local variables */
let get_locals pdesc => pdesc.attributes.locals;


/** Return name and type of captured variables */
let get_captured pdesc => pdesc.attributes.captured;


/** Return the visibility attribute */
let get_access pdesc => pdesc.attributes.access;

let get_nodes pdesc => pdesc.nodes;

let get_proc_name pdesc => pdesc.attributes.proc_name;


/** Return the return type of the procedure */
let get_ret_type pdesc => pdesc.attributes.ret_type;

let get_ret_var pdesc => Pvar.mk Ident.name_return (get_proc_name pdesc);

let get_start_node pdesc => pdesc.start_node;


/** List of nodes in the procedure sliced by a predicate up to the first branching */
let get_sliced_slope pdesc f =>
  Node.get_generated_slope (get_start_node pdesc) (fun n => Node.get_sliced_succs n f);


/** List of nodes in the procedure up to the first branching */
let get_slope pdesc => Node.get_generated_slope (get_start_node pdesc) Node.get_succs;


/** Return [true] iff the procedure is defined, and not just declared */
let is_defined pdesc => pdesc.attributes.is_defined;

let is_java_synchronized pdesc => pdesc.attributes.is_java_synchronized_method;

let iter_nodes f pdesc => IList.iter f (IList.rev (get_nodes pdesc));

let fold_calls f acc pdesc => {
  let do_node a node =>
    IList.fold_left
      (fun b callee_pname => f b (callee_pname, Node.get_loc node)) a (Node.get_callees node);
  IList.fold_left do_node acc (get_nodes pdesc)
};


/** iterate over the calls from the procedure: (callee,location) pairs */
let iter_calls f pdesc => fold_calls (fun _ call => f call) () pdesc;

let iter_instrs f pdesc => {
  let do_node node => IList.iter (fun i => f node i) (Node.get_instrs node);
  iter_nodes do_node pdesc
};

let fold_nodes f acc pdesc => IList.fold_left f acc (IList.rev (get_nodes pdesc));

let fold_instrs f acc pdesc => {
  let fold_node acc node =>
    IList.fold_left (fun acc instr => f acc node instr) acc (Node.get_instrs node);
  fold_nodes fold_node acc pdesc
};

let iter_slope f pdesc => {
  let visited = ref NodeSet.empty;
  let rec do_node node => {
    visited := NodeSet.add node !visited;
    f node;
    switch (Node.get_succs node) {
    | [n] =>
      if (not (NodeSet.mem n !visited)) {
        do_node n
      }
    | _ => ()
    }
  };
  do_node (get_start_node pdesc)
};

let iter_slope_calls f pdesc => {
  let do_node node => IList.iter (fun callee_pname => f callee_pname) (Node.get_callees node);
  iter_slope do_node pdesc
};


/** iterate between two nodes or until we reach a branching structure */
let iter_slope_range f src_node dst_node => {
  let visited = ref NodeSet.empty;
  let rec do_node node => {
    visited := NodeSet.add node !visited;
    f node;
    switch (Node.get_succs node) {
    | [n] =>
      if (not (NodeSet.mem n !visited) && not (Node.equal node dst_node)) {
        do_node n
      }
    | _ => ()
    }
  };
  do_node src_node
};


/** Set the exit node of the proc desc */
let set_exit_node pdesc node => pdesc.exit_node = node;


/** Set a flag for the proc desc */
let set_flag pdesc key value => proc_flags_add pdesc.attributes.proc_flags key value;


/** Set the start node of the proc desc */
let set_start_node pdesc node => pdesc.start_node = node;


/** Append the locals to the list of local variables */
let append_locals pdesc new_locals =>
  pdesc.attributes.locals = pdesc.attributes.locals @ new_locals;


/** Set the successor nodes and exception nodes, and build predecessor links */
let set_succs_exn_base (node: Node.t) succs exn => {
  node.succs = succs;
  node.exn = exn;
  IList.iter (fun (n: Node.t) => n.preds = [node, ...n.preds]) succs
};


/** Create a new cfg node */
let create_node pdesc loc kind instrs => {
  pdesc.nodes_num = pdesc.nodes_num + 1;
  let node_id = pdesc.nodes_num;
  let node = {
    Node.id: node_id,
    dist_exit: None,
    instrs,
    kind,
    loc,
    preds: [],
    pname: Some pdesc.attributes.proc_name,
    succs: [],
    exn: []
  };
  pdesc.nodes = [node, ...pdesc.nodes];
  node
};


/** Set the successor and exception nodes.
    If this is a join node right before the exit node, add an extra node in the middle,
    otherwise nullify and abstract instructions cannot be added after a conditional. */
let node_set_succs_exn pdesc (node: Node.t) succs exn =>
  switch (node.kind, succs) {
  | (Join_node, [{Node.kind: Exit_node _} as exit_node]) =>
    let kind = Node.Stmt_node "between_join_and_exit";
    let node' = create_node pdesc node.loc kind node.instrs;
    set_succs_exn_base node [node'] exn;
    set_succs_exn_base node' [exit_node] exn
  | _ => set_succs_exn_base node succs exn
  };
