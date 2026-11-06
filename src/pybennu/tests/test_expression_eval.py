import sys

import pytest
from py_expression_eval import Parser

# =============================================================================
# Mathematical Arithmetic Tests
# =============================================================================


def test_arithmetic_basic_operations():
    parser = Parser()

    # Addition (+)
    assert parser.parse("10 + 5").evaluate({}) == 15

    # Subtraction (-)
    assert parser.parse("20 - 7").evaluate({}) == 13

    # Multiplication (*)
    assert parser.parse("6 * 8").evaluate({}) == 48

    # Division (/)
    assert parser.parse("36 / 4").evaluate({}) == 9

    # Exponentiation (^)
    assert parser.parse("2 ^ 5").evaluate({}) == 32


def test_arithmetic_precedence_and_parentheses():
    parser = Parser()

    # Precedence: * before +
    assert parser.parse("2 + 3 * 4").evaluate({}) == 14

    # Parentheses override precedence
    assert parser.parse("(2 + 3) * 4").evaluate({}) == 20

    # Exponentiation precedence over multiplication
    assert parser.parse("2 * 3 ^ 2").evaluate({}) == 18

    # Unary negative
    assert parser.parse("-5 + 10").evaluate({}) == 5


# =============================================================================
# Boolean and Relational Expressions Tests
# =============================================================================


def test_relational_expressions():
    parser = Parser()

    # Greater than (>) and less than (<)
    assert parser.parse("5 > 3").evaluate({}) in (True, 1)
    assert parser.parse("3 < 5").evaluate({}) in (True, 1)
    assert parser.parse("5 < 3").evaluate({}) in (False, 0)
    assert parser.parse("3 > 5").evaluate({}) in (False, 0)

    # Equality (==)
    assert parser.parse("4 == 4").evaluate({}) in (True, 1)
    assert parser.parse("4 == 5").evaluate({}) in (False, 0)

    # Inequality (!=)
    assert parser.parse("4 != 5").evaluate({}) in (True, 1)
    assert parser.parse("4 != 4").evaluate({}) in (False, 0)


def test_boolean_logical_expressions():
    parser = Parser()

    # and
    assert parser.parse("1 and 1").evaluate({}) in (True, 1)
    assert parser.parse("1 and 0").evaluate({}) in (False, 0)

    # or
    assert parser.parse("0 or 1").evaluate({}) in (True, 1)
    assert parser.parse("0 or 0").evaluate({}) in (False, 0)

    # not
    assert parser.parse("not 0").evaluate({}) in (True, 1)
    assert parser.parse("not 1").evaluate({}) in (False, 0)

    # Combined boolean and relational
    expr = parser.parse("x > 2 and y < 10")
    assert expr.evaluate({"x": 5, "y": 8}) in (True, 1)
    assert expr.evaluate({"x": 1, "y": 8}) in (False, 0)
    assert expr.evaluate({"x": 5, "y": 15}) in (False, 0)


# =============================================================================
# Variable Substitution Tests
# =============================================================================


def test_variable_substitution():
    parser = Parser()
    expr = parser.parse("a + b * c")

    assert set(expr.variables()) == {"a", "b", "c"}
    result = expr.evaluate({"a": 10, "b": 2, "c": 3})
    assert result == 16

    # Test Expression.substitute
    subbed = expr.substitute("a", "x * 2")
    assert "x" in subbed.variables()
    assert subbed.evaluate({"x": 5, "b": 2, "c": 3}) == 16


def test_undefined_variable_raises():
    parser = Parser()
    expr = parser.parse("a + b")
    with pytest.raises(Exception, match="undefined variable"):
        expr.evaluate({"a": 1})


# =============================================================================
# Syntax Error Handling Tests
# =============================================================================


@pytest.mark.parametrize(
    "invalid_expr",
    [
        "2 + * 3",
        "((1 + 2)",
        "",
        ")",
        "* 5",
    ],
)
def test_syntax_errors(invalid_expr):
    parser = Parser()
    with pytest.raises(Exception, match="parse error"):
        parser.parse(invalid_expr)


# =============================================================================
# Python 3.14 Compatibility Guard Tests
# =============================================================================


def test_python_compatibility_guard():
    """Ensure py_expression_eval Parser runs cleanly across supported Python versions."""
    assert sys.version_info >= (3, 10)

    parser = Parser()
    # Complex expression combining arithmetic, variables, and logic
    expr = parser.parse("(x + y) * z > 10 and not w")
    res = expr.evaluate({"x": 2, "y": 3, "z": 4, "w": 0})
    assert res in (True, 1)
