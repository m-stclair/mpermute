from setuptools import Extension, setup

mpermute = Extension(
    "mpermute._mpermute",
    sources=["mpermute/_mpermute.c"],
    extra_compile_args=["-g"]
)

setup(ext_modules=[mpermute])
