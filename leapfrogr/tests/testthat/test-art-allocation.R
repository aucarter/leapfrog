test_that("ART allocation method 4 initiates the lowest CD4 stage first", {
  pjnz <- system.file("pjnz/bwa_aim-adult-art-no-special-elig_v6.13_2022-04-18.PJNZ", package = "leapfrog")
  years <- 1970:2030
  p <- process_pjnz(pjnz, use_coarse_age_groups = TRUE, extract_child_params = TRUE)
  ## no special-population or pregnant-women eligibility (this file has none)
  n_years <- dim(p$basepop)[3]
  p[c("specpop_percelig", "specpop_percelig_by_sex", "pw_artelig", "who34percelig")] <-
    list(rep(0, n_years), matrix(0, 2, n_years), rep(0L, n_years), 0)
  expect_type(p$art_alloc_method, "integer")
  run <- function(method) run_model(modifyList(p, list(art_alloc_method = as.integer(method))), "CoarseChildModel", years)
  m1 <- run(1)
  m4 <- run(4)

  ## Every method other than 4 is the weighted mix
  expect_equal(run(3)$h_artpop, m1$h_artpop)

  ## During scale-up more of those on ART started at the lowest CD4 stage, and
  ## fewer people die untreated
  y <- which(years %in% 2005:2015)
  lowest_share <- function(o) colSums(o$h_artpop[, 7, , , y], dims = 3) / colSums(o$h_artpop[, , , , y], dims = 4)
  expect_true(all(lowest_share(m4) > lowest_share(m1)))
  expect_lt(sum(m4$h_hiv_deaths_no_art[, , , y]), sum(m1$h_hiv_deaths_no_art[, , , y]))
})
