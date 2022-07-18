from pyln.client import Millibronee
import pytest  # type: ignore


def test_to_approx_str():
    amount = Millibronee('10000000bro')
    assert amount.to_approx_str() == "0.1bron"
    amount = Millibronee('1000000bro')
    assert amount.to_approx_str() == "0.01bron"
    amount = Millibronee('100000bro')
    assert amount.to_approx_str() == "0.001bron"
    amount = Millibronee('10000bro')
    assert amount.to_approx_str() == "10000bro"
    amount = Millibronee('1000bro')
    assert amount.to_approx_str() == "1000bro"
    amount = Millibronee('100mbro')
    assert amount.to_approx_str() == "0.1bro"

    # also test significant rounding
    amount = Millibronee('10001234bro')
    assert amount.to_approx_str() == "0.1bron"
    amount = Millibronee('1234bro')
    assert amount.to_approx_str(3) == "1234bro"  # note: no rounding
    amount = Millibronee('1234bro')
    assert amount.to_approx_str(2) == "1234bro"  # note: no rounding
    amount = Millibronee('1230bro')
    assert amount.to_approx_str(2) == "1230bro"  # note: no rounding
    amount = Millibronee('12345678bro')
    assert amount.to_approx_str() == "0.123bron"
    amount = Millibronee('12345678bro')
    assert amount.to_approx_str(1) == "0.1bron"
    amount = Millibronee('15345678bro')
    assert amount.to_approx_str(1) == "0.2bron"
    amount = Millibronee('1200000000bro')
    assert amount.to_approx_str() == "12bron"
    amount = Millibronee('1200000000bro')
    assert amount.to_approx_str(1) == "12bron"  # note: no rounding


def test_floats():
    # test parsing amounts from floating number strings
    amount = Millibronee("0.01bron")
    assert amount.to_bronee() == 10**6
    amount = Millibronee("1.01bron")
    assert amount.to_bronee() == 10**8 + 10**6
    amount = Millibronee("0.1bro")
    assert int(amount) == 100
    amount = Millibronee("0.01bro")
    assert int(amount) == 10
    amount = Millibronee("1.1bro")
    assert int(amount) == 1100

    # test floating point arithmetic
    amount = Millibronee("1000mbro") * 0.1
    assert int(amount) == 100
    amount = Millibronee("100mbro") * 0.1
    assert int(amount) == 10
    amount = Millibronee("10mbro") * 0.1
    assert int(amount) == 1


def test_zero():
    # zero amounts are of course valid
    amount = Millibronee("0bron")
    assert int(amount) == 0
    amount = Millibronee("0bro")
    assert int(amount) == 0
    amount = Millibronee("0mbro")
    assert int(amount) == 0

    # zero floating amount as well
    amount = Millibronee("0.0bron")
    assert int(amount) == 0
    amount = Millibronee("0.0bro")
    assert int(amount) == 0
    amount = Millibronee("0.0mbro")
    assert int(amount) == 0

    # also anything multiplied by zero
    amount = Millibronee("1bron") * 0
    assert int(amount) == 0
    amount = Millibronee("1bro") * 0
    assert int(amount) == 0
    amount = Millibronee("1mbro") * 0
    assert int(amount) == 0

    # and multiplied by a floating zero
    amount = Millibronee("1bron") * 0.0
    assert int(amount) == 0
    amount = Millibronee("1bro") * 0.0
    assert int(amount) == 0
    amount = Millibronee("1mbro") * 0.0
    assert int(amount) == 0


def test_round_zero():
    # everything below 1mbro should round down to zero
    amount = Millibronee("1mbro") * 0.9
    assert int(amount) == 0
    amount = Millibronee("10mbro") * 0.09
    assert int(amount) == 0
    amount = Millibronee("100mbro") * 0.009
    assert int(amount) == 0
    amount = Millibronee("1000mbro") * 0.0009
    assert int(amount) == 0

    amount = Millibronee("1bro") * 0.0009
    assert int(amount) == 0
    amount = Millibronee("0.1bro") * 0.009
    assert int(amount) == 0
    amount = Millibronee("0.01bro") * 0.09
    assert int(amount) == 0
    amount = Millibronee("0.001bro") * 0.9
    assert int(amount) == 0

    amount = Millibronee("10bro") * 0.00009
    assert int(amount) == 0
    amount = Millibronee("100bro") * 0.000009
    assert int(amount) == 0
    amount = Millibronee("1000bro") * 0.0000009
    assert int(amount) == 0
    amount = Millibronee("10000bro") * 0.00000009
    assert int(amount) == 0
    amount = Millibronee("10000bro") * 0.00000009
    assert int(amount) == 0

    amount = Millibronee("1bron") * 0.000000000009
    assert int(amount) == 0
    amount = Millibronee("0.1bron") * 0.00000000009
    assert int(amount) == 0
    amount = Millibronee("0.01bron") * 0.0000000009
    assert int(amount) == 0
    amount = Millibronee("0.001bron") * 0.000000009
    assert int(amount) == 0
    amount = Millibronee("0.0001bron") * 0.00000009
    assert int(amount) == 0
    amount = Millibronee("0.00001bron") * 0.0000009
    assert int(amount) == 0
    amount = Millibronee("0.000001bron") * 0.000009
    assert int(amount) == 0
    amount = Millibronee("0.0000001bron") * 0.00009
    assert int(amount) == 0
    amount = Millibronee("0.00000001bron") * 0.0009
    assert int(amount) == 0
    amount = Millibronee("0.000000001bron") * 0.009
    assert int(amount) == 0
    amount = Millibronee("0.0000000001bron") * 0.09
    assert int(amount) == 0
    amount = Millibronee("0.00000000001bron") * 0.9
    assert int(amount) == 0


def test_round_down():
    # sub mbro significatns should be floored
    amount = Millibronee("2mbro") * 0.9
    assert int(amount) == 1
    amount = Millibronee("20mbro") * 0.09
    assert int(amount) == 1
    amount = Millibronee("200mbro") * 0.009
    assert int(amount) == 1
    amount = Millibronee("2000mbro") * 0.0009
    assert int(amount) == 1

    amount = Millibronee("2bro") * 0.0009
    assert int(amount) == 1
    amount = Millibronee("0.2bro") * 0.009
    assert int(amount) == 1
    amount = Millibronee("0.02bro") * 0.09
    assert int(amount) == 1
    amount = Millibronee("0.002bro") * 0.9
    assert int(amount) == 1

    amount = Millibronee("20bro") * 0.00009
    assert int(amount) == 1
    amount = Millibronee("200bro") * 0.000009
    assert int(amount) == 1
    amount = Millibronee("2000bro") * 0.0000009
    assert int(amount) == 1
    amount = Millibronee("20000bro") * 0.00000009
    assert int(amount) == 1
    amount = Millibronee("20000bro") * 0.00000009
    assert int(amount) == 1

    amount = Millibronee("2bron") * 0.000000000009
    assert int(amount) == 1
    amount = Millibronee("0.2bron") * 0.00000000009
    assert int(amount) == 1
    amount = Millibronee("0.02bron") * 0.0000000009
    assert int(amount) == 1
    amount = Millibronee("0.002bron") * 0.000000009
    assert int(amount) == 1
    amount = Millibronee("0.0002bron") * 0.00000009
    assert int(amount) == 1
    amount = Millibronee("0.00002bron") * 0.0000009
    assert int(amount) == 1
    amount = Millibronee("0.000002bron") * 0.000009
    assert int(amount) == 1
    amount = Millibronee("0.0000002bron") * 0.00009
    assert int(amount) == 1
    amount = Millibronee("0.00000002bron") * 0.0009
    assert int(amount) == 1
    amount = Millibronee("0.000000002bron") * 0.009
    assert int(amount) == 1
    amount = Millibronee("0.0000000002bron") * 0.09
    assert int(amount) == 1
    amount = Millibronee("0.00000000002bron") * 0.9
    assert int(amount) == 1


def test_nosubmbro():
    # sub millibronee are not a concept yet
    with pytest.raises(ValueError, match='Millibronee must be a whole number'):
        Millibronee("0.1mbro")
    with pytest.raises(ValueError, match='Millibronee must be a whole number'):
        Millibronee(".1mbro")
    with pytest.raises(ValueError, match='Millibronee must be a whole number'):
        Millibronee("0.0001bro")
    with pytest.raises(ValueError, match='Millibronee must be a whole number'):
        Millibronee(".0001bro")
    with pytest.raises(ValueError, match='Millibronee must be a whole number'):
        Millibronee("0.000000000001bron")
    with pytest.raises(ValueError, match='Millibronee must be a whole number'):
        Millibronee(".000000000001bron")


def test_nonegative():
    with pytest.raises(ValueError, match='Millibronee must be >= 0'):
        Millibronee("-1bron")
    with pytest.raises(ValueError, match='Millibronee must be >= 0'):
        Millibronee("-1.0bron")
    with pytest.raises(ValueError, match='Millibronee must be >= 0'):
        Millibronee("-0.1bron")
    with pytest.raises(ValueError, match='Millibronee must be >= 0'):
        Millibronee("-.1bron")
    with pytest.raises(ValueError, match='Millibronee must be >= 0'):
        Millibronee("-1bro")
    with pytest.raises(ValueError, match='Millibronee must be >= 0'):
        Millibronee("-1.0bro")
    with pytest.raises(ValueError, match='Millibronee must be >= 0'):
        Millibronee("-0.1bro")
    with pytest.raises(ValueError, match='Millibronee must be >= 0'):
        Millibronee("-.1bro")
    with pytest.raises(ValueError, match='Millibronee must be >= 0'):
        Millibronee("-1mbro")
    with pytest.raises(ValueError, match='Millibronee must be >= 0'):
        Millibronee("-1.0mbro")

    with pytest.raises(ValueError, match='Millibronee must be >= 0'):
        Millibronee("1mbro") * -1
    with pytest.raises(ValueError, match='Millibronee must be >= 0'):
        Millibronee("1mbro") * -42
    with pytest.raises(ValueError, match='Millibronee must be >= 0'):
        Millibronee("1bro") * -1
    with pytest.raises(ValueError, match='Millibronee must be >= 0'):
        Millibronee("1bron") * -1

    with pytest.raises(ValueError, match='Millibronee must be >= 0'):
        Millibronee("1mbro") / -1
    with pytest.raises(ValueError, match='Millibronee must be >= 0'):
        Millibronee("1mbro") / -0.5
    with pytest.raises(ValueError, match='Millibronee must be >= 0'):
        Millibronee("1bro") / -1
    with pytest.raises(ValueError, match='Millibronee must be >= 0'):
        Millibronee("1bron") / -1

    with pytest.raises(ValueError, match='Millibronee must be >= 0'):
        Millibronee("1mbro") // -1
    with pytest.raises(ValueError, match='Millibronee must be >= 0'):
        Millibronee("1bro") // -1
    with pytest.raises(ValueError, match='Millibronee must be >= 0'):
        Millibronee("1bron") // -1


def test_mul():
    # mbro * num := mbro
    amount = Millibronee(21) * 2
    assert isinstance(amount, Millibronee)
    assert amount == Millibronee(42)
    amount = Millibronee(21) * 2.5
    assert amount == Millibronee(52)

    # mbro * mbro := mbro^2  (which is not supported)
    with pytest.raises(TypeError, match="not supported"):
        Millibronee(21) * Millibronee(2)


def test_div():
    # mbro / num := mbro
    amount = Millibronee(42) / 2
    assert isinstance(amount, Millibronee)
    assert amount == Millibronee(21)
    amount = Millibronee(42) / 2.6
    assert amount == Millibronee(16)

    # mbro / mbro := num   (float ratio)
    amount = Millibronee(42) / Millibronee(2)
    assert isinstance(amount, float)
    assert amount == 21.0
    amount = Millibronee(8) / Millibronee(5)
    assert amount == 1.6

    # mbro // num := mbro
    amount = Millibronee(42) // 2
    assert isinstance(amount, Millibronee)
    assert amount == Millibronee(21)

    # mbro // mbro := num
    amount = Millibronee(42) // Millibronee(3)
    assert isinstance(amount, int)
    assert amount == 14
    amount = Millibronee(42) // Millibronee(3)
    assert amount == 14
    amount = Millibronee(42) // Millibronee(4)
    assert amount == 10


def test_init():
    # Note: Ongoing Discussion, hence the `with pytest.raises`.
    # https://github.com/ElementsProject/lightning/pull/4273#discussion_r540369093
    #
    # Initialization with a float should be possible:
    # Millibronee(5) / 2 currently works, and removes the half mbro.
    # So Millibronee(5 / 2) should be the same.
    amount = Millibronee(5) / 2
    assert amount == Millibronee(2)
    with pytest.raises(TypeError, match="Millibronee by float is currently not supported"):
        assert amount == Millibronee(5 / 2)

    ratio = Millibronee(8) / Millibronee(5)
    assert isinstance(ratio, float)
    with pytest.raises(TypeError, match="Millibronee by float is currently not supported"):
        assert Millibronee(ratio) == Millibronee(8 / 5)

    # Check that init by a round float is allowed.
    # Required by some existing tests: tests/test_wallet.py::test_txprepare
    amount = Millibronee(42.0)
    assert amount == 42
