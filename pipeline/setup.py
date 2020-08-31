''' File to compile cython code 

Usage: python setup.py build_ext --inplace
Then import cython module inside python interpretor

'''

import distutils.core
import Cython.Build
distutils.core.setup(
    ext_modules = Cython.Build.cythonize(["process_data_np.pyx",
                                          #"process_stk_audio_np.pyx"
                                          ], compiler_directives={'language_level': 3}))

