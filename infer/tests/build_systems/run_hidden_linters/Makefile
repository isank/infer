# Copyright (c) 2016-present, Facebook, Inc.
#
# This source code is licensed under the MIT license found in the
# LICENSE file in the root directory of this source tree.

# this test checks that hidden linters do not run without --no-filtering,
# regardless of what other flags are used to enable them (e.g. --compute-analytics)

TESTS_DIR = ../..

ANALYZER = linters

CODETOANALYZE_DIR = ../codetoanalyze/componentkit

CLANG_OPTIONS = -x objective-c++ -std=c++11 -c -fblocks
INFER_OPTIONS = --compute-analytics --project-root $(CODETOANALYZE_DIR)
INFERPRINT_OPTIONS = --issues-tests

SOURCES = $(CODETOANALYZE_DIR)/TestComponentKitAnalytics.mm

include $(TESTS_DIR)/clang.make
