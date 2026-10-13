#!/usr/bin/env Rscript

## This script creates test data required for the leapfrog tests.
## We read some input data and prepare a set of demographic projection
## and HIV parameters for both the adult and the child model.
## We also run leapfrog and save out the result for use in reference tests
setwd("leapfrogr")
devtools::load_all()

# nolint start
library(dplyr)

## Create demographic and projection parameters for adults
pjnz_adult <- file.path(here::here(), "inst", "pjnz", "bwa_aim-adult-art-no-special-elig_v6.13_2022-04-18.PJNZ")

parameters <- process_pjnz(pjnz_adult, extract_child_params = FALSE)
save_parameters(parameters, testthat::test_path("testdata/adult_parms_full.h5"))

parameters_coarse <- process_pjnz(pjnz_adult, extract_child_params = FALSE, use_coarse_age_groups = TRUE)
save_parameters(parameters_coarse, testthat::test_path("testdata/adult_parms_coarse.h5"))

# We use France for testing Spectrum model variant as it has non-zero Non-AIDS excess mortality
# inputs. Which we need for testing modelled non-AIDS excess mortality.
# This was created by creating a new default projection in Spectrum
pjnz_france <- file.path(here::here(), "inst", "pjnz", "france_default.PJNZ")
france_parameters <- process_pjnz(pjnz_france, extract_child_params = TRUE)
save_parameters(france_parameters, testthat::test_path("testdata/spectrum_params.h5"))

#Create paeds parameters
pjnz_child <- file.path(here::here(), "inst", "pjnz", "bwa_aim-no-special-elig-numpmtct.PJNZ")

parameters <- process_pjnz(pjnz_child, extract_child_params = TRUE,
                           bypass_adult = TRUE)
parameters_coarse <- process_pjnz(pjnz_child, extract_child_params = TRUE,
                                  use_coarse_age_groups = TRUE, bypass_adult = TRUE)

pop1 <- gsub(pattern = '.PJNZ', replacement = '_pop1.xlsx', x = pjnz_child)

# Copied from EPPASM to remove cyclic dependency between leapfrog and eppasm
read_pop1 <- function(pop1file, country, years = 2000:2021){

  ## Read population out of Spectrum file
  pop_list <- list()
  for(yr in years){
    pop1 <- as.data.frame(readxl::read_excel(pop1file, sheet=as.character(yr)))
    pop1 <- pop1[-1,]
    names(pop1)[1:3] <- c("sex", "cd4", "artdur")
    pop1 <- stats::reshape(pop1, idvar=c("sex", "cd4", "artdur"), varying=c(0:79, "80+"), times=c(0:79, "80+"), timevar="age", v.names="pop", direction="long")
    pop1$year <- as.integer(yr)
    pop1$age <- as.integer(gsub("\\+", "", pop1$age))
    pop1$cd4 <- as.integer(gsub("CD4 Count: ", "", pop1$cd4))
    pop1$artdur <- as.integer(gsub("Duration: ", "", pop1$artdur))
    pop_list[[as.character(yr)]] <- pop1
  }
  pop <- plyr::rbind.fill(pop_list)
  pop$sex <- gsub("ales", "ale", pop$sex)
  pop <- subset(pop, cd4 != 0 & artdur != 0 & pop > 0)

  pop$country <- country

  return(pop)
}

spectrum_output <- function(file, ages = 0:14, country = 'Botswana', years_in = 1970:2030){
  ##pull out stratified population from the .xlsx file, This function doesn't take out the paediatric output, so going to just compare to the Spectrum software itself
  df <- file

  df <- read_pop1(df, country, years = years_in)
    df_paed <- df %>% dplyr::filter(age < 5) %>%
      dplyr::right_join(y = data.frame(cd4 = 1:8, cd4_cat = c('neg', 'gte30', '26-30', '21-25', '16-20', '11-15', '5-10', 'lte5'))) %>%
      dplyr::right_join(y = data.frame(artdur = 2:8, transmission = c('perinatal', 'bf0-6', 'bf7-12', 'bf12+', 'ARTlte5mo', 'ART6to12mo', 'ARTgte12mo'))) %>%
      dplyr::filter(cd4_cat != 'neg')

    df_adol <- df %>% dplyr::filter(age > 4 & age < 15) %>%
      dplyr::right_join(y = data.frame(cd4 = 3:8, cd4_cat = c('gte1000', '750-999', '500-749', '350-499', '200-349','lte200'))) %>%
      dplyr::right_join(y = data.frame(artdur = 2:8, transmission = c('perinatal', 'bf0-6', 'bf7-12', 'bf12+', 'ARTlte5mo', 'ART6to12mo', 'ARTgte12mo'))) %>%
      dplyr::filter(cd4_cat != 'neg')

    df_paed <- rbind(df_paed, df_adol)

    df <- df %>% dplyr::filter(age %in%  c(15:max(ages))) %>%
      dplyr::right_join(y = data.frame(cd4 = 2:8, cd4_cat = c('gte500', '350-500', '250-349', '200-249', '100-199','50-99', 'lte50'))) %>%
      dplyr::right_join(y = data.frame(artdur = 2:8, transmission = c('perinatal', 'bf0-6', 'bf7-12', 'bf12+', 'ARTlte5mo', 'ART6to12mo', 'ARTgte12mo'))) %>%
      dplyr::filter(cd4_cat != 'neg')

    df <- rbind(df, df_paed)
  # setnames(df, 'cd4', 'cd4_cat')
  df <- df %>% dplyr::select(sex, age, cd4_cat, year, pop, transmission)

  df_on_treatment <- df[grepl('ART', df$transmission),] %>% dplyr::select(sex, age, cd4_cat, year, pop, transmission) %>% dplyr::group_by(sex, age, cd4_cat, transmission, year) %>% dplyr::mutate(pop = sum(pop)) %>% unique() %>% dplyr::filter(age %in% ages)
  df_off_treatment <- df[!grepl('ART', df$transmission),]%>% dplyr::filter(age %in% ages)
  df_total <- df %>% dplyr::select(sex, age, cd4_cat, year, pop) %>% dplyr::group_by(sex, age, cd4_cat, year) %>% dplyr::mutate(pop = sum(pop)) %>% unique()%>% dplyr::filter(age %in% ages)

  return(list(on_treatment = df_on_treatment, off_treatment = df_off_treatment, total = df_total))

}
df <- spectrum_output(pop1, ages = 0:80, 'country', years_in = 1970:2030)
x <- df$total

dp <- pjnz:::read_dp_raw(pjnz_child)
dat <- pjnz::read_dp(pjnz_child)

aids_deathsnoart <- array(0, dim = c(15,2,61), dimnames = list(age = 0:14, sex = c('male', 'female'), years = 1970:2030))
aids_deathsnoart[,'male',] <- dat$data$aids_deaths_no_art_single_age$data[as.character(0:14),"male",]
aids_deathsnoart[,'female',] <- dat$data$aids_deaths_no_art_single_age$data[as.character(0:14),"female",]

aids_deathsart <- array(0, dim = c(15,2,61), dimnames = list(age = 0:14, sex = c('male', 'female'), years = 1970:2030))
aids_deathsart[,'male',] <- dat$data$aids_deaths_art_single_age$data[as.character(0:14),"male",]
aids_deathsart[,'female',] <- dat$data$aids_deaths_art_single_age$data[as.character(0:14),"female",]

spec_ctx_need <- dat$data$child_art_calc$data["both", "Children needing cotrim (0-14): ",]

out <- list(dp = dp,
            pjnz = pjnz_child,
            pop1 = x,
            ontrt = df$on_treatment,
            offtrt = df$off_treatment,
            deaths_noart = aids_deathsnoart,
            deaths_art = aids_deathsart,
            ctx_need = as.numeric(unlist(spec_ctx_need)))

save_parameters(parameters, testthat::test_path("testdata/child_parms_full.h5"))
save_parameters(parameters_coarse, testthat::test_path("testdata/child_parms_coarse.h5"))

# need this for child model tests
saveRDS(out, testthat::test_path("testdata/child_test_utils.rds"))

## Create synthetic HivStructuralRate parameters. The adult PJNZ (with its
## child block) is grown to the age-10 / 0-105 state space, and the sexual
## debut and sexual-risk class inputs are made up, so no real country data
## enters the fixture beyond the PJNZ already in the package.
extend_array_age <- function(x, age_dim = 1, value = NULL, n_ages = 25,
                             location = "after") {
  x_dim <- dim(x)
  if (is.null(value)) {
    x_sub <- lapply(x_dim, seq_len)
    x_sub[[age_dim]] <- x_dim[age_dim]
    value <- do.call("[", c(list(x), x_sub))
  }
  value <- array(value, replace(x_dim, age_dim, 1))
  extend_sub <- lapply(replace(x_dim, age_dim, n_ages), seq_len)
  extend_sub[[age_dim]][] <- 1
  value_extend <- do.call("[", c(list(value), extend_sub, drop = FALSE))
  if (location == "after") {
    abind::abind(x, value_extend, along = age_dim)
  } else {
    abind::abind(value_extend, x, along = age_dim)
  }
}

sr_parameters <- process_pjnz(pjnz_adult, extract_child_params = TRUE)
n_years <- dim(sr_parameters$basepop)[3]

# Demography: ages 81-105 are empty (Sx carries the open-age rate forward)
# and fertility starts at age 10 with zero rates at 10-14
sr_parameters$basepop <- extend_array_age(sr_parameters$basepop, value = 0)
sr_parameters$netmigr_adj <- extend_array_age(sr_parameters$netmigr_adj, value = 0)
sr_parameters$Sx <- extend_array_age(sr_parameters$Sx)
for (nm in c("asfr", "hc_age_specific_fertility_rate",
             "adult_female_infections", "adult_female_hivnpop")) {
  sr_parameters[[nm]] <- extend_array_age(sr_parameters[[nm]], value = 0,
                                          n_ages = 5, location = "before")
}
sr_parameters$fert_rat <- extend_array_age(sr_parameters$fert_rat,
                                           value = sr_parameters$fert_rat[1, ],
                                           n_ages = 5, location = "before")
sr_parameters$frr_art6mos <- c(rep(sr_parameters$frr_art6mos[1], 5),
                               sr_parameters$frr_art6mos)

# HIV arrays: carry the open age to 105, then prepend ages 10-14
hiv_age_dim <- c(incrr_age = 1, cd4_mort = 2, cd4_nonaids_excess_mort = 2,
                 cd4_prog = 2, cd4_initdist = 2, art_mort = 3,
                 art_nonaids_excess_mort = 3)
for (nm in names(hiv_age_dim)) {
  sr_parameters[[nm]] <- extend_array_age(sr_parameters[[nm]], age_dim = hiv_age_dim[[nm]])
  sr_parameters[[nm]] <- extend_array_age(sr_parameters[[nm]], age_dim = hiv_age_dim[[nm]],
                                          n_ages = 5, location = "before")
}
sr_parameters$incrr_age[1:5, , ] <- 0.6^(5:1) %o% sr_parameters$incrr_age[6, , ]

# The child model ends at age 10
sr_parameters$hc_art_val <- sr_parameters$hc_art_val[1:3, ]
sr_parameters$hc_art_elig_age <- pmin(sr_parameters$hc_art_elig_age, 10L)

# Structural rate r(t), seeded in 1975 and peaking around 1990
t_hts <- seq(0, by = 0.1, length.out = length(sr_parameters$transmission_rate_hts))
sr_parameters$transmission_rate_hts <- 0.04 + 0.12 * exp(-((t_hts - 20) / 10)^2)
sr_parameters$epidemic_start_hts <- 50L
sr_parameters$initial_incidence <- 0.0005

# Sexual debut over ages 10-29
debut_age <- 10:29
debut <- cbind(plogis((debut_age - 18) / 1.5), plogis((debut_age - 17) / 1.5))
sr_parameters$sexdebut_annual_prob <- array(debut, c(20, 2, n_years))

# Five sexual-risk classes: no sex, one cohabiting partner and non-regular
# partners 1 / 2 / 3+. Interventions reduce transmission by up to 40% from 2000.
young <- plogis((30 - 0:105) / 5)
class_prop <- cbind(0.2, 0.5 * (1 - young) + 0.2, 0.15 * young + 0.05,
                    0.05 * young + 0.02, 0.03 * young + 0.01)
class_prop <- class_prop / rowSums(class_prop)
intervention_mult <- pmin(1, pmax(0.6, 1 - 0.02 * (seq_len(n_years) - 31)))
sr_parameters$sr_contact_rate <- c(0, 0.5, 1.5, 3, 6)
sr_parameters$sr_class_prop <- aperm(array(class_prop, c(106, 5, 2, n_years)), c(1, 3, 2, 4))
sr_parameters$sr_intervention_mult <- array(rep(intervention_mult, each = 106 * 2 * 5),
                                            c(106, 2, 5, n_years))

# Key populations aged 15-49: female sex workers, MSM, PWID
sr_parameters$sr_kp_prop <- array(0, c(106, 2, 3, n_years))
sr_parameters$sr_kp_prop[16:50, 2, 1, ] <- 0.01
sr_parameters$sr_kp_prop[16:50, 1, 2, ] <- 0.008
sr_parameters$sr_kp_prop[16:50, , 3, ] <- 0.002
sr_parameters$sr_kp_irr <- matrix(c(10, 20, 15), 3, n_years)
sr_parameters$sr_kp_intervention_mult <- array(rep(intervention_mult, each = 106 * 2 * 3),
                                               c(106, 2, 3, n_years))

save_parameters(sr_parameters, testthat::test_path("testdata/sr_parms.h5"))

# R output for the same file, which the leapfrog-py tests compare against
sr_output <- run_model(read_parameters(testthat::test_path("testdata/sr_parms.h5")),
                       "HivStructuralRate")
save_hdf5_file(sr_output, testthat::test_path("testdata/sr_r_output.h5"))
