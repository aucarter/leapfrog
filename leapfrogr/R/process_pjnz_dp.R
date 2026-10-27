process_pjnz_dp <- function(dat, pars, dim_vars) {
  basepop <- pars$big_pop[, c("male", "female"), ]
  sx <- pars$surv_rate

  proportion_of_male_births <- pars$sex_birth_ratio / (pars$sex_birth_ratio + 100)
  births_sex_prop <- rbind(
    male = proportion_of_male_births,
    female = 1 - proportion_of_male_births
  )

  asfr <- pars$asfr / 100
  asfr <- apply(asfr / 5, 2, rep, each = 5)
  asfr_sum <- colSums(asfr)
  asfr_sum[asfr_sum == 0] <- 1
  asfr <- sweep(asfr, 2, pars$tfr / asfr_sum, "*")
  dimnames(asfr) <- list(age = 15:49, year = dim_vars$years)

  migr_age_dist <- pars$migr_age_dist / 100
  migr_age_dist_sum <- colSums(migr_age_dist)
  migr_age_dist_sum[migr_age_dist_sum == 0] <- 1
  migr_age_dist <- sweep(migr_age_dist, 2:3, migr_age_dist_sum, "/")
  netmigr_5year <- sweep(migr_age_dist, 2:3, pars$migr_rate, "*")

  netmigr <- array(
    dim = c(length(dim_vars$a_80plus_no_80), length(dim_vars$s), length(dim_vars$years)),
    dimnames = list(
      age = dim_vars$a_80plus_no_80,
      sex = dim_vars$s,
      year = dim_vars$years
    )
  )
  netmigr[1:80, , ] <- apply(netmigr_5year[1:16, , ], 2:3, beers::beers_sub_ordinary)
  netmigr[81, , ] <- netmigr_5year[17, , ]

  u5prop <- array(dim = c(5, 2))
  u5prop[1, ] <- sx[1, , 1] * 2
  u5prop[2, ] <- sx[2, , 1] * u5prop[1, ]
  u5prop[3, ] <- sx[3, , 1] * u5prop[2, ]
  u5prop[4, ] <- sx[4, , 1] * u5prop[3, ]
  u5prop[5, ] <- sx[5, , 1] * u5prop[4, ]
  u5prop <- sweep(u5prop, 2, colSums(u5prop), "/")

  netmigr[1:5, 1, ] <- u5prop[, 1, drop = FALSE] %*% netmigr_5year[1, 1, ]
  netmigr[1:5, 2, ] <- u5prop[, 2, drop = FALSE] %*% netmigr_5year[1, 2, ]

  ## valid_vers records the version number that PJNZ was saved with
  pars$projection_period <- if (pars$valid_vers > "6.2") "calendar" else "midyear"

  projection_period <- if (pars$version_num > "6.2") "calendar" else "midyear"
  netmigr_adj <- if (projection_period == "midyear") {
    adj <- netmigr
    adj[-1, , ] <- (netmigr[-1, , ] + netmigr[-81, , ]) / 2
    adj[1, , ] <- netmigr[1, , ] / 2
    adj[81, , ] <- adj[81, , ] + netmigr[81, , ] / 2
    adj
  } else {
    netmigr
  }

  ## Spectrum's regional population adjustment (RegionalAdjustPopCBState = 1):
  ## the population is rescaled to this series at the end of each projection
  ## year up to CustomPopStopRescalingYear. Simulate with a *PopAdjust model
  ## configuration when these are present.
  population_adjustment <- if (isTRUE(pars$regional_adjust_pop_state == 1)) {
    list(
      targetpop = pars$regional_adjust_pop,
      population_adjustment_final_step =
        as.integer(pars$custom_pop_stop_rescaling_year - pars$first_year)
    )
  }

  c(
    list(
      projection_start_year = pars$first_year,
      projection_period = projection_period,
      basepop = basepop,
      Sx = sx,
      netmigr_adj = netmigr_adj,
      asfr = asfr,
      births_sex_prop = births_sex_prop,
      tfr = pars$tfr
    ),
    population_adjustment
  )
}
