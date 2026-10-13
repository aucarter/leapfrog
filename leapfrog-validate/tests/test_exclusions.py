import pytest

from leapfrog_validate.exclusions import Exclusion, exclusion_mask


def test_exclusion_requires_a_reason():
    with pytest.raises(ValueError, match="reason"):
        Exclusion(pjnz="kenya", reason="", link="https://example.com/issue/1")


def test_exclusion_requires_a_link():
    with pytest.raises(ValueError, match="link"):
        Exclusion(pjnz="kenya", reason="known upstream rounding quirk", link="")


def test_exclusion_rejects_whitespace_only_reason_or_link():
    with pytest.raises(ValueError, match="reason"):
        Exclusion(pjnz="kenya", reason="   ", link="https://example.com/issue/1")
    with pytest.raises(ValueError, match="link"):
        Exclusion(pjnz="kenya", reason="known quirk", link="   ")


def test_exclusion_with_reason_and_link_is_accepted():
    excl = Exclusion(pjnz="kenya", reason="known quirk", link="https://example.com/issue/1")
    assert excl.pjnz == "kenya"


def test_exclusion_mask_only_applies_to_matching_pjnz():
    excl = Exclusion(pjnz="kenya", reason="r", link="l")
    mask = exclusion_mask((2, 2, 3), [excl], pjnz="uganda")
    assert not mask.any()


def test_exclusion_mask_with_no_region_excludes_everything_for_matching_pjnz():
    excl = Exclusion(pjnz="kenya", reason="r", link="l")
    mask = exclusion_mask((2, 2, 3), [excl], pjnz="kenya")
    assert mask.all()


def test_exclusion_mask_region_only_excludes_matching_cells():
    excl = Exclusion(pjnz="kenya", reason="r", link="l", age=(0, 0))
    mask = exclusion_mask((2, 2, 3), [excl], pjnz="kenya")
    assert mask[:, :, 0].all()
    assert not mask[:, :, 1:].any()


def test_exclusion_mask_combines_multiple_entries():
    excl_a = Exclusion(pjnz="kenya", reason="r", link="l", age=(0, 0))
    excl_b = Exclusion(pjnz="kenya", reason="r", link="l", age=(2, 2))
    mask = exclusion_mask((2, 2, 3), [excl_a, excl_b], pjnz="kenya")
    assert mask[:, :, 0].all()
    assert not mask[:, :, 1].any()
    assert mask[:, :, 2].all()


def test_exclusion_mask_year_and_sex_axes_are_independently_filterable():
    excl = Exclusion(pjnz="kenya", reason="r", link="l", year=(1, 1), sex=(0, 0))
    mask = exclusion_mask((2, 2, 3), [excl], pjnz="kenya")
    assert mask[1, 0, :].all()
    assert not mask[0, :, :].any()
    assert not mask[1, 1, :].any()


def test_exclusion_mask_supports_a_year_only_indicator():
    """Year-only indicators (`pmtct_need`, `cotrim_need`) have no sex or age axis."""
    excl = Exclusion(pjnz="kenya", reason="r", link="l", year=(1, 2))
    mask = exclusion_mask((4,), [excl], pjnz="kenya")
    assert mask.tolist() == [False, True, True, False]


def test_exclusion_mask_rejects_a_sex_or_age_bound_on_a_year_only_indicator():
    excl = Exclusion(pjnz="kenya", reason="r", link="l", age=(0, 0))
    with pytest.raises(ValueError, match="no age axis"):
        exclusion_mask((4,), [excl], pjnz="kenya")
