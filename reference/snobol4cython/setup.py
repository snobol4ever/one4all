from setuptools import setup, Extension

setup(
    name="snobol4c",
    version="0.1.0",
    ext_modules=[
        Extension(
            "snobol4c",
            sources=["snobol4c_module.c"],
            extra_compile_args=["-O2", "-Wall", "-Wno-unused-function"],
        )
    ],
)
