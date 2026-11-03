test_that("ART allocation method 4 initiates the lowest CD4 stage first", {
  pjnz <- system.file("pjnz/bwa_aim-no-special-elig-numpmtct.PJNZ", package = "leapfrog")
  years <- 1970:2030
  p <- process_pjnz(pjnz, use_coarse_age_groups = TRUE, extract_child_params = TRUE)
  expect_type(p$art_alloc_method, "integer")
  run <- function(method) run_model(modifyList(p, list(art_alloc_method = as.integer(method))), "CoarseChildModel", years)
  m1 <- run(1)
  m4 <- run(4)

  ## Every method other than 4 is the weighted mix
  expect_equal(run(3)$h_artpop, m1$h_artpop)

  ## Same number on ART, drawn from the sickest: fewer untreated people at the
  ## lowest CD4 stage and fewer untreated deaths during scale-up
  y <- which(years %in% 2005:2015)
  expect_equal(colSums(m4$h_artpop, dims = 4)[y], colSums(m1$h_artpop, dims = 4)[y], tolerance = 1e-3)
  expect_true(all(colSums(m4$h_hivpop[7, , , y], dims = 2) < colSums(m1$h_hivpop[7, , , y], dims = 2)))
  expect_lt(sum(m4$h_hiv_deaths_no_art[, , , y]), sum(m1$h_hiv_deaths_no_art[, , , y]))
})
