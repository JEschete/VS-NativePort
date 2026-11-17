"""F02805 _setEnemyDescription, MENU9.PRG 238.c:1414."""

from gazette_terminal_common import DESCRIPTION, configure

globals().update(configure(2805, DESCRIPTION, 0x80104B40, 0x80104BD0, {
    "read-previous-instead-of-unlocked": {0x80104B70: 0x9042000D},
    "invert-unlocked-choice": {0x80104B78: 0x14400009},
    "description-offset-is-not-bytes": {0x80104B94: 0x00042080},
    "locked-name-instead-of-description": {0x80104BA8: 0x94440006},
    "omit-information-endpoint": {0x80104BB8: 0},
}))
