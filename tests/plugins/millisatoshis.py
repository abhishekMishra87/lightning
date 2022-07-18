#!/usr/bin/env python3
from pyln.client import Plugin, Millibronee


plugin = Plugin(autopatch=True)


@plugin.method("echo")
def echo(plugin, mbro: Millibronee, not_an_mbro):
    plugin.log("got echo for {} {} (types {} and {})"
               .format(mbro, not_an_mbro, type(mbro), type(not_an_mbro)))
    if not isinstance(mbro, Millibronee):
        raise TypeError("mbro must be Millibronee not {}".format(type(mbro)))
    if isinstance(not_an_mbro, Millibronee):
        raise TypeError("not_an_mbro must not be Millibronee")
    plugin.log("got echo for {} (type {})".format(mbro, type(mbro)))
    if not isinstance(mbro, Millibronee):
        raise TypeError("mbro must be Millibronee not {}".format(type(mbro)))
    plugin.log("Returning {}".format(mbro))
    return {'echo_mbro': mbro}


plugin.run()
