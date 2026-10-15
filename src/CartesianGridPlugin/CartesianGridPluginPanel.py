# Copyright (C) 2026  CARTESIANGRIDPLUGIN authors
# LGPL 2.1 or later
# The C++ grid panel of the CartesianGridParameters hypothesis dialog, embedded in a PyQt dialog:
# the C entry points of libCartesianGridPluginGUI are called with ctypes and the C++ widget is
# wrapped as a PyQt widget with sip.wrapinstance (same Qt, same process). Needs the SALOME GUI.

import ctypes
import os

import salome
from qtsalome import QWidget
from PyQt5 import sip

LIBRARY = "libCartesianGridPluginGUI.so"
_BUFFER = 4096

_lib = None

def libraryPath():
    return os.path.join(os.environ.get("CARTESIANGRIDPLUGIN_ROOT_DIR", ""), "lib", "salome", LIBRARY)

def _library():
    """the GUI library with its C entry points declared; OSError if it cannot be loaded"""
    global _lib
    if _lib is None:
        # lazy binding, as SMESH loads it: some indirect dependencies have unresolved symbols
        lib = ctypes.CDLL(libraryPath(), mode=os.RTLD_LAZY)
        lib.CartesianGridPanel_New.restype = ctypes.c_void_p
        lib.CartesianGridPanel_New.argtypes = [ctypes.c_void_p]
        text = [ctypes.c_char_p, ctypes.c_int]
        lib.CartesianGridPanel_SetShape.argtypes = [ctypes.c_void_p, ctypes.c_char_p] + text
        lib.CartesianGridPanel_Load.argtypes = [ctypes.c_void_p, ctypes.c_char_p] + text
        lib.CartesianGridPanel_Check.argtypes = [ctypes.c_void_p] + text
        lib.CartesianGridPanel_Store.argtypes = [ctypes.c_void_p, ctypes.c_char_p] + text + text
        for f in (lib.CartesianGridPanel_SetShape, lib.CartesianGridPanel_Load,
                  lib.CartesianGridPanel_Check, lib.CartesianGridPanel_Store):
            f.restype = ctypes.c_int
        _lib = lib
    return _lib

class GridPanel:
    """Per-axis tabs and outside cells policy; widget is the PyQt QWidget to put in a layout.
    The C++ panel is a child of parent and is deleted with it"""
    def __init__(self, parent):
        self._lib = _library()
        self._ptr = self._lib.CartesianGridPanel_New(sip.unwrapinstance(parent))
        if not self._ptr:
            raise RuntimeError("Cannot create the grid panel")
        self.widget = sip.wrapinstance(self._ptr, QWidget)

    def _call(self, f, *args):
        err = ctypes.create_string_buffer(_BUFFER)
        if not f(self._ptr, *args, err, _BUFFER):
            raise RuntimeError(err.value.decode("utf-8", "replace"))

    @staticmethod
    def _ior(hyp):
        return salome.orb.object_to_string(hyp).encode()

    def setShape(self, entry):
        """extents of the study object entry for the actual cells; RuntimeError if unreadable"""
        self._call(self._lib.CartesianGridPanel_SetShape, entry.encode())

    def load(self, hyp):
        """fill the panel from a CartesianGridParameters hypothesis"""
        self._call(self._lib.CartesianGridPanel_Load, self._ior(hyp))

    def check(self):
        """RuntimeError with the message (the faulty tab is shown) if an axis is invalid"""
        self._call(self._lib.CartesianGridPanel_Check)

    def store(self, hyp):
        """write the panel to hyp; return a summary of the grid"""
        summary = ctypes.create_string_buffer(_BUFFER)
        self._call(self._lib.CartesianGridPanel_Store, self._ior(hyp), summary, _BUFFER)
        return summary.value.decode("utf-8", "replace")
