#!/usr/bin/env python3

"""Run leapfrog model and save output to specified dir
Usage:
  run_model <params> <configuration> <output-dir>

Arguments:
  <params>         Params h5 filename under ../leapfrogr/tests/testthat/testdata/ to run.
  <configuration>  Model configuration to run (e.g. HivFullAgeStratification,
                   HivCoarseAgeStratification, Spectrum).
  <output-dir>     Path to save output to.

Options:
  -h --help                  Show this screen.
"""

import os

from docopt import docopt

from leapfrog_py import read_h5_file, run_model, save_h5_file

if __name__ == "__main__":
    args = docopt(__doc__)
    params = args["<params>"]
    configuration = args["<configuration>"]
    output_dir = args["<output-dir>"]

    if not os.path.exists(output_dir):
        os.mkdir(output_dir)

    parameters = read_h5_file(
        os.path.join("..", "leapfrogr", "tests", "testthat", "testdata", params)
    )
    ret = run_model(parameters, configuration)

    save_h5_file(ret, os.path.join(output_dir, "py-output.h5"))
