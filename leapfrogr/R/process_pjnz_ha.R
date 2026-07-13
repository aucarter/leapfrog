hiv_age_span_params <- function(dat, pars, dim_vars) {
  h_ag_span_coarse <- c(2L, 3L, 5L, 5L, 5L, 5L, 5L, 5L, 31L)
  frr_agecat <- seq(15, 45, 5)
  h_fert_idx <- which((15L - 1 + cumsum(h_ag_span_coarse)) %in% 15:49)
  age_band_width <- length(15:49) / length(frr_agecat)
  fert_rat_h_ag <- findInterval(
    15L + cumsum(h_ag_span_coarse[h_fert_idx]) - h_ag_span_coarse[h_fert_idx],
    frr_agecat
  )

  fert_rat_coarse <- array(1, c(length(h_fert_idx), length(dim_vars$years)))
  fert_rat_coarse[] <- pars$hivtfr[fert_rat_h_ag, ]
  fert_rat_full <- apply(pars$hivtfr, 2, rep, each = age_band_width)

  ratio_women_on_art <- if (is.null(pars$ratio_women_on_art)) {
    rep(1, length(dim_vars$a_15to49_5year))
  } else if (dat$data$ratio_women_on_art$tag == "RatioWomenOnART MV") {
    rep(pars$ratio_women_on_art, length(dim_vars$a_15to49_5year))
  } else {
    pars$ratio_women_on_art
  }

  list(
    h_ag_span_coarse = h_ag_span_coarse,
    h_fert_idx = h_fert_idx,
    fert_rat_coarse = fert_rat_coarse,
    fert_rat_full = fert_rat_full,
    frr_art6mos_coarse = array(ratio_women_on_art[fert_rat_h_ag]),
    frr_art6mos_full = rep(ratio_women_on_art, each = age_band_width)
  )
}

process_pjnz_ha <- function(dat, pars, dim_vars, use_coarse_age_groups = FALSE) {
  incidinput <- if (!is.null(pars$incidence_by_fit) && !is.null(pars$incidence_options)) {
    pars$incidence_by_fit[pars$incidence_options + 1, ]
  } else if (!is.null(pars$incidence_input)) {
    pars$incidence_input
  } else {
    stop("Could not calculate incidinput")
  }
  incidinput <- incidinput / 100

  hiv_steps_per_year <- 10

  incidence_model_choice <- 0L
  transmission_rate_hts <- numeric(length(incidinput) * hiv_steps_per_year)
  initial_incidence <- 0.0
  relative_infectiousness_art <- 0.1
  epidemic_start_hts <- length(transmission_rate_hts)

  incrr_age <- pars$dist_of_hiv

  ## Note from Rob Glaubius (9 Dec 2020)
  ## Sometime between version 5.756 and the release v5.87 the variable <HIVSexRatio MV>
  ## was dropped for the more detailed <SexRatioByEpidPatt MV>. The old tag was
  ## reintroduced more recently with the shifts in cell placement you noticed. Since the
  ## variable was removed and reintroduced we didn't think to tag this as "MV2" instead
  ## of "MV".
  ##
  ## Action: to parse this check that one of these rows 2 or 3 after the <HIVSexRatio MV>
  ## has data, but not both.
  time_idx <- 4:(4 + length(dim_vars$years) - 1)
  incrr_sex <- if ("<HIVSexRatio MV>" %in% dat$dp_raw$Tag) {
    hiv_sex_ratio_idx <- which(dat$dp_raw[, 1] == "<HIVSexRatio MV>")
    first_entries <- dat$dp_raw[hiv_sex_ratio_idx + 2:3, 4]
    data_row <- which(first_entries != "" & !is.na(first_entries))
    if (length(data_row) != 1) {
      stop("DP tag <HIVSexRatio MV> not parsed correctly")
    }
    array(
      as.numeric(unlist(dat$dp_raw[hiv_sex_ratio_idx + data_row + 1, time_idx])),
      dim = length(dim_vars$years),
      dimnames = list(dim_vars$years)
    )
  } else if ("<SexRatioByEpidPatt MV>" %in% dat$dp_raw$Tag) {
    sexincrr_idx <- as.integer(dat$dp_raw[which(dat$dp_raw[, 1] == "<IncEpidemicRGIdx MV>") + 2, 4]) # 0 based indexing
    data <- dat$dp_raw[which(dat$dp_raw[, 1] == "<SexRatioByEpidPatt MV>") + 3:8, time_idx][sexincrr_idx + 1, ]
    array(
      as.numeric(data),
      dim = length(dim_vars$years),
      dimnames = list(dim_vars$years)
    )
  } else {
    stop("Incidence sex ratio not found")
  }

  ## Hard coded to expand age groups 15-24, 25-34, 35-44, 45+ to
  ## single-year ages 15:80.
  ## Requires extension for coarse HIV age group stratification
  idx_expand_full <- rep(1L, 66L)
  idx_expand_coarse <- rep(1:4, times = c(3, 2, 2, 2))
  idx_expand <- if (use_coarse_age_groups) idx_expand_coarse else idx_expand_full

  cd4_mort <- aperm(pars$adult_mort_by_cd4_no_art, c(2, 3, 1))
  cd4_initdist <- aperm(pars$adult_dist_new_infections_cd4, c(2, 3, 1)) / 100
  cd4_prog <- aperm(pars$adult_ann_rate_progress_lower_cd4, c(2, 3, 1))

  vers_str <- sub("^([0-9]+),(.*)$", "\\1.\\2", pars$valid_vers)
  version <- as.numeric(sub("^([0-9\\.]+).*", "\\1", vers_str))
  beta_version <- ifelse(
    grepl("Beta", vers_str),
    as.numeric(sub(".*Beta ([0-9]+)$", "\\1", vers_str)),
    NA
  )
  scale_cd4_mort <- ifelse(
    version >= 5.73 && (beta_version >= 15 || is.na(beta_version)),
    1L,
    0L
  )

  ## eligibility starts in projection year idx
  artcd4elig_idx <- findInterval(
    -pars$cd4_threshold_adults,
    -c(999, 500, 350, 250, 200, 100, 50)
  )
  # Update eligibility threshold from CD4 <200 to <250 to account for additional
  # proportion eligible with WHO Stage 3/4.
  artcd4elig_idx <- replace(artcd4elig_idx, artcd4elig_idx == 5L, 4L)

  # create new dimension and combine arrays along it so now dims are:
  # sex, cd4 count, age groups, art duration
  art_mort <- abind::abind(
    pars$adult_mort_by_cd4_with_art_0to6,
    pars$adult_mort_by_cd4_with_art_7to12,
    pars$adult_mort_by_cd4_with_art_gt12,
    along = 4
  )
  # dims are now: art duration, cd4 count, age groups, sex
  art_mort <- aperm(art_mort, c(4, 2, 3, 1))

  artmx_timerr <- array(
    1, c(3, length(dim_vars$years)), list(
      artdur = c("ART0MOS", "ART6MOS", "ART1YR"),
      year = dim_vars$years
    )
  )
  if (!is.null(pars$mortality_rates)) {
    if (dat$data$mortality_rates$tag == "MortalityRates MV") {
      artmx_timerr["ART0MOS", ] <- pars$mortality_rates
      artmx_timerr["ART6MOS", ] <- pars$mortality_rates
      artmx_timerr["ART1YR", ] <- pars$mortality_rates
    } else if (dat$data$mortality_rates$tag == "MortalityRates MV2") {
      artmx_timerr["ART0MOS", ] <- pars$mortality_rates[1, ]
      artmx_timerr["ART6MOS", ] <- pars$mortality_rates[1, ]
      artmx_timerr["ART1YR", ] <- pars$mortality_rates[2, ]
    }
  }

  cd4_nonaids_excess_mort <- array(0, dim(cd4_mort), dimnames(cd4_mort))
  art_nonaids_excess_mort <- array(0, dim(cd4_mort), dimnames(cd4_mort))
  if (!is.null(pars$adult_non_aids_excess_mort)) {
    cd4_nonaids_excess_mort <- aperm(pars$adult_non_aids_excess_mort[1, , , ], c(2, 3, 1))
    art_nonaids_excess_mort <- aperm(pars$adult_non_aids_excess_mort[2, , , ], c(2, 3, 1))
  }

  art_dropout_rate <- -log(1.0 - pars$perc_lost_followup / 100)

  art15plus_numperc <- pars$ha_art_by_sex_per_num[c("male", "female"), ]
  art15plus_num <- pars$ha_art_by_sex[c("male", "female"), ]

  adult_artadj_factor <- array(1, dim(art15plus_num))
  adult_artadj_absolute <- array(0, dim(art15plus_num))
  art_factor_flag <- is.null(pars$adult_art_adj_factor_flag) || pars$adult_art_adj_factor_flag == 1
  if (!is.null(pars$adult_art_adj_factor) && art_factor_flag) {
    adult_artadj_factor <- pars$adult_art_adj_factor
    if (!is.null(pars$adult_pats_alloc_to_from_other_region)) {
      adult_artadj_absolute <- pars$adult_pats_alloc_to_from_other_region
    }
    adult_artadj_factor <- adult_artadj_factor ^ as.numeric(!art15plus_numperc)
    adult_artadj_absolute <- adult_artadj_absolute * as.numeric(!art15plus_numperc)
    art15plus_num <- art15plus_num + adult_artadj_absolute
    art15plus_num <- art15plus_num * adult_artadj_factor
  }

  art15plus_isperc <- art15plus_numperc == 1
  art15plus_num[art15plus_isperc] <- art15plus_num[art15plus_isperc] / 100

  art_alloc_mxweight <- pars$new_art_pat_alloc[1]

  p_ag_15to49 <- 35L
  p_ag_15plus <- 66L
  pAG_INCIDPOP <- ifelse(pars$epp_population_ages == 0L, p_ag_15to49, p_ag_15plus)
  pIDX_INCIDPOP <- 15L

  fp <- hiv_age_span_params(dat, pars, dim_vars)

  cd4fert_rat <- if (is.null(pars$fert_cd4_discount)) {
    rep(1, length(dim_vars$cd4_count))
  } else {
    pars$fert_cd4_discount
  }

  frr_scalar <- if (is.null(pars$frr_by_location)) 1 else pars$frr_by_location
  fert_rat <- if (use_coarse_age_groups) fp$fert_rat_coarse else fp$fert_rat_full
  frr_art6mos <- if (use_coarse_age_groups) fp$frr_art6mos_coarse else fp$frr_art6mos_full

  proj_years_count <- as.integer(pars$final_year - pars$first_year + 1L)
  t_ART_start <- min(c(unlist(apply(art15plus_num > 0, 1, which)), proj_years_count))

  ## Use Beer's coefficients to distribution IRRs by age/sex
  incrr_age <- apply(incrr_age, 2:3, beers_open_ended)[16:81, , ] ## !! Hard coded
  incrr_age[incrr_age < 0] <- 0

  h_ts <- 3
  cd4_initdist <- cd4_initdist[, idx_expand, ]
  cd4_prog <- (1 - exp(-cd4_prog[, idx_expand, ] / hiv_steps_per_year)) * hiv_steps_per_year
  cd4_mort <- cd4_mort[, idx_expand, ]

  artmx_multiplier <- if (is.null(pars$mortality_rates_multiplier)) 1 else pars$mortality_rates_multiplier
  art_mort <- artmx_multiplier * art_mort[c(1, 2, 3), , idx_expand, ]
  cd4_nonaids_excess_mort <- cd4_nonaids_excess_mort[, idx_expand, ]
  art_nonaids_excess_mort_hts <- array(0.0, dim(art_mort), dimnames(art_mort))
  art_nonaids_excess_mort_hts[] <- rep(art_nonaids_excess_mort[, idx_expand, ], each = h_ts)
  art_nonaids_excess_mort <- art_nonaids_excess_mort_hts

  art_dropout_recover_cd4 <- vers_str >= "6.14"

  if (!is.null(pars$pwid_hivpos_nonaids_mortality) && !is.na(pars$pwid_hivpos_nonaids_mortality[1])) {
    # This is hardcoding which epp_idu_morality to pass through
    # it copies Spectrum which is assuming the the first subpop
    # will be PWID.
    pwid_hivpos_nonaids_mortality <- pars$pwid_hivpos_nonaids_mortality[1]
    if (length(pars$pwid_sex_ratio) < proj_years_count) {
      pwid_sex_ratio <- rep(pars$pwid_sex_ratio, proj_years_count)
    } else {
      pwid_sex_ratio <- pars$pwid_sex_ratio
    }
  } else {
    pwid_sex_ratio <- rep(-1, proj_years_count)
    pwid_hivpos_nonaids_mortality <- -1
  }

  list(
    incidence_model_choice = incidence_model_choice,
    incidinput = incidinput,
    transmission_rate_hts = transmission_rate_hts,
    initial_incidence = initial_incidence,
    epidemic_start_hts = epidemic_start_hts,
    relative_infectiousness_art = relative_infectiousness_art,
    incrr_age = incrr_age,
    incrr_sex = incrr_sex,
    cd4_mort = cd4_mort,
    cd4_prog = cd4_prog,
    cd4_initdist = cd4_initdist,
    scale_cd4_mort = scale_cd4_mort,
    artcd4elig_idx = artcd4elig_idx,
    art_mort = art_mort,
    artmx_timerr = artmx_timerr,
    cd4_nonaids_excess_mort = cd4_nonaids_excess_mort,
    art_nonaids_excess_mort = art_nonaids_excess_mort,
    art_dropout_recover_cd4 = art_dropout_recover_cd4,
    art_dropout_rate = art_dropout_rate,
    art15plus_num = art15plus_num,
    art15plus_isperc = art15plus_isperc,
    art_alloc_mxweight = art_alloc_mxweight,
    pAG_INCIDPOP = pAG_INCIDPOP,
    pIDX_INCIDPOP = pIDX_INCIDPOP,
    fert_rat = fert_rat,
    cd4fert_rat = cd4fert_rat,
    frr_art6mos = frr_art6mos,
    frr_scalar = frr_scalar,
    pwid_hivpos_nonaids_mortality = pwid_hivpos_nonaids_mortality,
    pwid_prop_hivpop = pars$pwid_prop_hivpop,
    pwid_sex_ratio = pwid_sex_ratio,
    t_ART_start = t_ART_start
  )
}
