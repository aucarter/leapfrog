#!/usr/bin/env Rscript

usage <- "Run leapfrog model and save output to specified dir
Usage:
  run_model <params> <configuration> <output-dir>

Arguments:
  <params>         Params h5 filename under tests/testthat/testdata/ to run.
  <configuration>  Model configuration to run, see leapfrog::list_model_configurations().
  <output-dir>     Path to save output to.

Options:
  -h --help                  Show this screen.
"

dat <- docopt::docopt(usage)
names(dat) <- gsub("-", "_", names(dat), fixed = TRUE)

if (!dir.exists(dat$output_dir)) {
  dir.create(dat$output_dir, recursive = TRUE)
}

parameters <- leapfrog::read_parameters(testthat::test_path(file.path("testdata", dat$params)))
out <- leapfrog::run_model(parameters, configuration = dat$configuration)

leapfrog:::save_hdf5_file(out, file.path(dat$output_dir, "r-output.h5"))
