from pyln.client import Millibronee


def test_sum_radd():
    result = sum([Millibronee(1), Millibronee(2), Millibronee(3)])
    assert int(result) == 6
