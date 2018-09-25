from enum import Enum


class Chipset(Enum):
    ATHEROS = 'ath9k_htc'
    RALINK = 'rt2800usb'
    REALTEK = 'rtlxxx'
    MEDIATEK = 'mt7601u'


def isatheros_card(drivername):
    if drivername.startswith('ath'):
        return True
    else:
        return False


def isralink_card(drivername):
    if drivername.startswith('rt') and drivername[2].isnumeric():
        return True
    else:
        return False


def isrealtek_card(drivername):
    if drivername.startswith('rtl'):
        return True
    else:
        return False
