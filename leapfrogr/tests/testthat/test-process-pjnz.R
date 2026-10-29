bwa_pjnz <- system_file(
  "pjnz/bwa_aim-adult-art-no-special-elig_v6.13_2022-04-18.PJNZ"
)

bwa_pmtct_pjnz <- system_file(
  "pjnz/bwa_aim-no-special-elig-numpmtct.PJNZ"
)

test_that("process_pjnz includes all required parameters", {
  pars <- process_pjnz(bwa_pjnz)

  ## DP
  expect_true(all(c("basepop", "Sx", "netmigr_adj", "asfr", "tfr",
                    "births_sex_prop", "projection_period",
                    "projection_start_year") %in% names(pars)))

  ## HIV adult
  expect_true(all(c("incidinput", "incrr_age", "incrr_sex",
                    "cd4_mort", "cd4_prog", "cd4_initdist",
                    "art_mort", "artmx_timerr", "artcd4elig_idx",
                    "art_dropout_rate", "art15plus_num", "art15plus_isperc",
                    "scale_cd4_mort", "incidence_model_choice") %in% names(pars)))

  ## Parameter dimensions are correct
  proj_years <- length(pars$tfr)
  expect_equal(dim(pars$basepop), c(81, 2, proj_years), ignore_attr = TRUE)
  expect_equal(dim(pars$cd4_mort), c(7, 66, 2), ignore_attr = TRUE)
  expect_equal(dim(pars$artmx_timerr),      c(3, proj_years),   ignore_attr = TRUE)

  ## DP params are in valid ranges
  expect_true(all(pars$Sx >= 0 & pars$Sx <= 1))
  expect_true(all(pars$basepop > 0))
  expect_true(all(pars$asfr >= 0))
  expect_true(all(pars$tfr > 0))
  # male + female birth proportions sum to 1 each year
  expect_equal(colSums(pars$births_sex_prop), rep(1, ncol(pars$births_sex_prop)),
               tolerance = 1e-10, ignore_attr = TRUE)
  expect_true(pars$projection_period %in% c("calendar", "midyear"))

  ## HIV adult params are in valid ranges
  expect_true(all(pars$incidinput >= 0 & pars$incidinput <= 1))
  expect_true(all(pars$incrr_age >= 0))
  expect_true(all(pars$incrr_sex > 0))
  # CD4 initial distribution sums to 1 over stages for each age/sex
  cd4_col_sums <- apply(pars$cd4_initdist, 2:3, sum)
  expect_equal(cd4_col_sums, matrix(1, 66, 2), tolerance = 1e-6, ignore_attr = TRUE)
  expect_true(all(pars$art_dropout_rate >= 0))
  # ART eligibility index must be within valid CD4 stage range (1-7)
  expect_true(all(pars$artcd4elig_idx >= 1L & pars$artcd4elig_idx <= 7L))
  expect_true(pars$scale_cd4_mort %in% c(0L, 1L))
})

test_that("process_pjnz use_coarse_age_groups changes HIV age stratification dimensions", {
  pars_full   <- process_pjnz(bwa_pjnz, use_coarse_age_groups = FALSE)
  pars_coarse <- process_pjnz(bwa_pjnz, use_coarse_age_groups = TRUE)

  expect_equal(dim(pars_full$cd4_mort),   c(7, 66, 2), ignore_attr = TRUE)
  expect_equal(dim(pars_coarse$cd4_mort), c(7,  9, 2), ignore_attr = TRUE)

  expect_equal(dim(pars_full$art_mort),   c(3, 7, 66, 2), ignore_attr = TRUE)
  expect_equal(dim(pars_coarse$art_mort), c(3, 7,  9, 2), ignore_attr = TRUE)

  # Spectrum's adult HIV parameters are by age 15-24, 25-34, 35-44, 45+: each
  # single age carries the value of the coarse group it falls in
  coarse_of_age <- rep(1:9, c(2, 3, 5, 5, 5, 5, 5, 5, 31))
  for (p in c("cd4_mort", "cd4_prog", "cd4_initdist")) {
    expect_equal(pars_full[[p]], pars_coarse[[p]][, coarse_of_age, ], ignore_attr = TRUE)
  }
  expect_equal(pars_full$art_mort, pars_coarse$art_mort[, , coarse_of_age, ], ignore_attr = TRUE)

  # Demographic parameters are unaffected by age stratification choice
  expect_equal(dim(pars_full$basepop), dim(pars_coarse$basepop))
  expect_equal(dim(pars_full$Sx),      dim(pars_coarse$Sx))
})

test_that("process_pjnz extract_child_params adds child-specific parameters", {
  pars_adult <- process_pjnz(bwa_pmtct_pjnz)
  pars_child <- process_pjnz(bwa_pmtct_pjnz, extract_child_params = TRUE)

  child_pars <- c("hc_nosocomial_infections_by_age", "hc1_cd4_dist", "hc1_cd4_mort",
                  "hc2_cd4_mort", "hc1_cd4_prog", "hc2_cd4_prog",
                  "cotrim_val", "PMTCT", "vertical_transmission_rate",
                  "pbfw_prep_clients", "pbfw_prep_is_percent",
                  "pbfw_prep_adherence_daily_oral", "pbfw_prep_adherence_injectable",
                  "pbfw_prep_client_incidence_ratio", "pbfw_prep_person_years_daily_oral",
                  "pbfw_prep_person_years_injectable")
  expect_true(all(child_pars %in% names(pars_child)))
  expect_false(any(child_pars %in% names(pars_adult)))

  # Adult parameters still present
  expect_true(all(c("basepop", "Sx", "cd4_mort") %in% names(pars_child)))
})

test_that("PrEP for pregnant and breastfeeding women passes through as zero for older PJNZ", {
  pars_child <- process_pjnz(bwa_pmtct_pjnz, extract_child_params = TRUE)

  expect_equal(nrow(pars_child$pbfw_prep_clients), 2L)
  expect_equal(ncol(pars_child$pbfw_prep_clients), ncol(pars_child$PMTCT),
               ignore_attr = TRUE)
  expect_equal(rownames(pars_child$pbfw_prep_clients), c("daily_oral", "injectable"))
  expect_true(all(pars_child$pbfw_prep_clients == 0))

  expect_equal(pars_child$pbfw_prep_is_percent,
               rep(0L, ncol(pars_child$PMTCT)), ignore_attr = TRUE)

  expect_equal(pars_child$pbfw_prep_adherence_daily_oral, 0)
  expect_equal(pars_child$pbfw_prep_adherence_injectable, 0)
  expect_equal(pars_child$pbfw_prep_client_incidence_ratio, 0)
  expect_equal(pars_child$pbfw_prep_person_years_daily_oral, 0)
  expect_equal(pars_child$pbfw_prep_person_years_injectable, 0)
})

test_that("PrEP for pregnant and breastfeeding women is read from PJNZ when present", {
  raw_dp <- pjnz::read_dp(bwa_pmtct_pjnz, include_raw = TRUE)
  n_years <- length(raw_dp$dim_vars$years)
  preg_prep <- matrix(0, nrow = 2, ncol = n_years,
                      dimnames = list(c("daily_oral", "injectable"), raw_dp$dim_vars$years))
  preg_prep["daily_oral", ] <- 10
  preg_prep["injectable", ] <- 3
  raw_dp$data$prep_for_pregnant_women <- list(data = preg_prep, tag = "AM_PMTCTReceivingOralPrEP MV")
  raw_dp$data$prep_parameters <- list(
    data = array(c(0.75, 0.9, 1, 0.59, 0.85), dim = 5L,
                 dimnames = list(c("adherence_daily_oral", "adherence_injectable",
                                   "client_incidence_ratio",
                                   "person_years_daily_oral", "person_years_injectable"))),
    tag = "AM_PMTCTPrEPParameters MV")

  with_mocked_bindings(
    pars_child <- process_pjnz(bwa_pmtct_pjnz, extract_child_params = TRUE),
    read_dp = function(...) raw_dp,
    .package = "pjnz"
  )

  expect_true(all(pars_child$pbfw_prep_clients["daily_oral", ] == 10))
  expect_true(all(pars_child$pbfw_prep_clients["injectable", ] == 3))
  # PJNZ always carries a count, never coverage
  expect_equal(pars_child$pbfw_prep_is_percent, rep(0L, n_years))
  expect_equal(pars_child$pbfw_prep_adherence_daily_oral, 0.75)
  expect_equal(pars_child$pbfw_prep_adherence_injectable, 0.9)
  expect_equal(pars_child$pbfw_prep_client_incidence_ratio, 1)
  expect_equal(pars_child$pbfw_prep_person_years_daily_oral, 0.59)
  expect_equal(pars_child$pbfw_prep_person_years_injectable, 0.85)
})

test_that("process_pjnz converts non-age stratified nosocomial infections to age stratified", {
  raw_dp <- pjnz::read_dp(bwa_pmtct_pjnz, include_raw = TRUE)
  raw_dp$data$nosocomial_infections <- list(
    data = rep(1, dim(raw_dp$data$nosocomial_infections_by_age$data)[2]),
    tag = "NoscomialInfections MV"
  )
  raw_dp$data$nosocomial_infections_by_age <- NULL

  with_mocked_bindings(
    pars_child <- process_pjnz(bwa_pmtct_pjnz, extract_child_params = TRUE),
    read_dp = function(...) raw_dp,
    .package = "pjnz"
  )

  expect_true(all(pars_child$hc_nosocomial_infections_by_age[1, ] == 1))
  expect_true(all(pars_child$hc_nosocomial_infections_by_age[2:15, ] == 0))
})

test_that("projection period follows the Spectrum version the file was saved with", {
  expect_equal(spectrum_projection_period("6.43"), "calendar")
  expect_equal(spectrum_projection_period("6,43"), "calendar")
  expect_equal(spectrum_projection_period("6.2"), "calendar")
  expect_equal(spectrum_projection_period("6.38 Beta 18"), "calendar")
  expect_equal(spectrum_projection_period("6.13"), "midyear")
  expect_equal(spectrum_projection_period("5.1"), "midyear")
  expect_error(spectrum_projection_period("unknown"), "Could not read")
test_that("incidence input carries Spectrum's incidence adjustment factor", {
  pjnz <- system.file("pjnz", "Botswana2018.PJNZ", package = "pjnz")
  skip_if(!nzchar(pjnz), "pjnz test file not available")
  dat <- pjnz::read_dp(pjnz)
  iaf <- dat$data$incidence_adjustment_factor$data
  skip_if(is.null(iaf), "installed pjnz does not read incidence_adjustment_factor")
  pars <- lapply(dat$data, `[[`, "data")
  raw <- (if (!is.null(pars$incidence_by_fit) && !is.null(pars$incidence_options)) {
    pars$incidence_by_fit[pars$incidence_options + 1, ]
  } else {
    pars$incidence_input
  }) / 100
  p <- process_pjnz(pjnz)
  expect_equal(p$incidinput, raw * iaf, ignore_attr = TRUE)
  expect_false(isTRUE(all.equal(p$incidinput, raw, check.attributes = FALSE)))
})

test_that("relative infectiousness on ART is read from the DP", {
  pjnz <- system.file("pjnz/bwa_aim-adult-art-no-special-elig_v6.13_2022-04-18.PJNZ", package = "leapfrog")
  dat <- pjnz::read_dp(pjnz)
  p <- process_pjnz(pjnz)
  expect_equal(p$relative_infectiousness_art, 1 - dat$data$adult_infect_reduc$data)
})
