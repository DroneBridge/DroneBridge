#
# This file is part of DroneBridge: https://github.com/seeul8er/DroneBridge
#
#   Copyright 2018 Wolfgang Christl
#
#   Licensed under the Apache License, Version 2.0 (the "License");
#   you may not use this file except in compliance with the License.
#   You may obtain a copy of the License at
#
#   http://www.apache.org/licenses/LICENSE-2.0
#
#   Unless required by applicable law or agreed to in writing, software
#   distributed under the License is distributed on an "AS IS" BASIS,
#   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
#   See the License for the specific language governing permissions and
#   limitations under the License.
#

from enum import Enum


class DBCommProt(Enum):
    DB_ORIGIN_GND = 'groundstation'
    DB_ORIGIN_UAV = 'drone'
    DB_TYPE_SETTINGS_SUCCESS = 'settingssuccess'
    DB_TYPE_ERROR = 'error'
    DB_TYPE_PING_REQUEST = 'pingrequest'
    DB_TYPE_PING_RESPONSE = 'pingresponse'
    DB_TYPE_SYS_IDENT_REQUEST = 'system_ident_req'
    DB_TYPE_SYS_IDENT_RESPONSE = 'system_ident_rsp'
    DB_TYPE_SETTINGS_CHANGE = 'settingschange'
    DB_TYPE_CAMSELECT = 'camselect'
    DB_TYPE_ADJUSTRC = 'adjustrc'
    DB_TYPE_MSP = 'mspcommand'
    DB_TYPE_ACK = 'ack'
    DB_TYPE_SETTINGS_REQUEST = 'settingsrequest'
    DB_TYPE_SETTINGS_RESPONSE = 'settingsresponse'
    DB_REQUEST_TYPE_WBC = 'wbc'
    DB_REQUEST_TYPE_DB = 'db'
