#' Prepare inputs from Spectrum PJNZ
#'
#' @param pjnz path to PJNZ file
#' @param use_coarse_age_groups use the coarse age stratification
#' @param extract_child_params If TRUE, then child parameters required
#'  to run the child model will be extracted. If FALSE, only adult parameters
#'  will be extracted from the PJNZ.
#' @param bypass_adult produce parameters that will bypass the adult
#'  model when running the child model variant
#'
#' @return list of input parameters
#'
#' @examples
#' pjnz <- system.file(
#'   "pjnz/bwa_aim-adult-art-no-special-elig_v6.13_2022-04-18.PJNZ",
#'   package = "leapfrog")
#' parameters <- process_pjnz(pjnz)
#'
#' @export
process_pjnz <- function(
    pjnz,
    use_coarse_age_groups = FALSE,
    extract_child_params = FALSE,
    bypass_adult = FALSE
) {
  dat <- pjnz::read_dp(pjnz, include_raw = TRUE)
  dim_vars <- dat$dim_vars

  pars <- lapply(dat$data, `[[`, "data")
  pars$country_code <- read_pjn_country_code(pjnz)

  dp_params <- process_pjnz_dp(dat, pars, dim_vars)
  ha_params <- process_pjnz_ha(dat, pars, dim_vars, use_coarse_age_groups)
  if (extract_child_params || bypass_adult) {
    hc_params <- process_pjnz_hc(dat, pars, dim_vars, dp_params, use_coarse_age_groups, bypass_adult)
  } else {
    hc_params <- list()
  }
  c(dp_params, ha_params, hc_params)
}

## ISO numeric country code from the PJN's "Country Code" row (NA if absent)
read_pjn_country_code <- function(pjnz) {
  pjn <- grep("\\.PJN$", utils::unzip(pjnz, list = TRUE)$Name, ignore.case = TRUE, value = TRUE)
  if (length(pjn) != 1) return(NA_integer_)
  con <- unz(pjnz, pjn)
  on.exit(close(con))
  rows <- strsplit(readLines(con, warn = FALSE, encoding = "latin1"), ",")
  i <- which(vapply(rows, function(r) length(r) > 1 && r[2] == "Country Code", logical(1)))
  if (length(i) != 1) return(NA_integer_)
  suppressWarnings(as.integer(rows[[i + 1]][4]))
}
