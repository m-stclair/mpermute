from setuptools import Extension, setup

mpermute = Extension(
    "mpermute.mpermute_core",
    sources=[
        "mpermute/mpermute_core.c", "mpermute/unique.c"
    ],
    extra_compile_args=["-g"]
)

setup(ext_modules=[mpermute])
