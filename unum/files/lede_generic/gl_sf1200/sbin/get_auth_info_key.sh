#!/bin/sh
# For Gl-Inet SF1200, the serial number is the auth_info_key
. m_functions.sh && echo -n $(get_sf1200_serial_num)
