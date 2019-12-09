import setuptools

with open("README.md", "r") as fh:
    long_description = fh.read()

setuptools.setup(
    name="DroneBridge",
    version="1.0.0",
    author="Wolfgang Christl",
    author_email="author@example.com",
    description="DroneBridge common library",
    long_description="Use DroneBridge to communicate with WiFi adapters set into monitor mode. DroneBridge will inject "
                     "packets using its own so called 'raw protocol'",
    long_description_content_type="text/markdown",
    url="https://github.com/DroneBridge",
    packages=setuptools.find_packages(),
    classifiers=[
        "Development Status :: 4 - Beta"
        "Programming Language :: Python :: 3",
        "License :: OSI Approved :: Apache Software License",
        "Operating System :: POSIX :: Linux",
    ],
    python_requires='>=3.6',
)
