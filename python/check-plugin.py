import os

from pxr import Plug

import logging
logging.basicConfig(level=logging.INFO)
logger = logging.getLogger("CHECK-PLUGIN")

logger.info(f'PXR_PLUGINPATH_NAME: {os.getenv("PXR_PLUGINPATH_NAME")}')

def check_plugin(plugin_name):
    plugin = Plug.Registry().GetPluginWithName(plugin_name)
    if plugin:
        plugin.Load()
        if plugin.isLoaded:
            logger.info(f'Plugin "{plugin_name}" is loaded.')
        else:
            logger.warning(f'Unable to load plugin "{plugin_name}"!')
    else:
        logger.error(f'Failed to find plugin "{plugin_name}"!')

if __name__ == "__main__":
    check_plugin("simpleDecimateSchema")
    check_plugin("simpleDecimateHydra2")