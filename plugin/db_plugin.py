import configparser
import os
import argparse
from subprocess import Popen


def parseArguments():
    parser = argparse.ArgumentParser(description='This DroneBridge module starts all plugins located in /boot/plugins')
    parser.add_argument('-g', action='store_true', dest='gnd',
                        help='start plugins for ground station - if not set we start plugins for UAV')
    return parser.parse_args()


def main():
    parsedArgs = parseArguments()
    if parsedArgs.gnd:
        command_section = 'ground'
    else:
        command_section = 'uav'

    plugin_dirs_list = next(os.walk(os.path.abspath(os.path.join(os.sep, 'boot', 'plugins'))))[1]
    for plugin_dir in plugin_dirs_list:
        config = configparser.ConfigParser()
        config.optionxform = str
        config.read(os.path.join(os.sep, 'boot', 'plugins', plugin_dir, 'settings.ini'))
        plugin_name = config.get('About', 'name')
        plugin_version = config.getint('About', 'version')
        plugin_author = config.get('About', 'author')
        # plugin_license = config.get('About', 'license')
        # plugin_website = config.get('About', 'website')
        plugin_startup_comm = config.get(command_section, 'startup_comm')
        if not plugin_startup_comm == "":
            print("DB_PLUGIN: Starting - " + plugin_name + " v" + str(plugin_version) + " by " + plugin_author)
            Popen([plugin_startup_comm], shell=True)


if __name__ == "__main__":
    main()
