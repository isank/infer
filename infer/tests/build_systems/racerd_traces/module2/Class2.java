/*
 * Copyright (c) 2017-present, Facebook, Inc.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

package threadsafety_traces.module2;

import threadsafety_traces.module1.Class1;

public class Class2 {

  public static void method() {
    Class1.method();
  }

}
