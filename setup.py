from setuptools import setup, find_packages

with open("README.md", "r", encoding="utf-8") as fh:
    long_description = fh.read()

setup(
    name="redistoon",
    version="0.1.0",
    author="DreamStack",
    author_email="dev@dreamstack.us",
    description="Python client for redisTOON - Redis module for Token-Oriented Object Notation",
    long_description=long_description,
    long_description_content_type="text/markdown",
    url="https://github.com/DreamStack-us/redisTOON",
    py_modules=["redistoon"],
    classifiers=[
        "Development Status :: 3 - Alpha",
        "Intended Audience :: Developers",
        "Topic :: Database",
        "Topic :: Software Development :: Libraries :: Python Modules",
        "License :: OSI Approved :: MIT License",
        "Programming Language :: Python :: 3",
        "Programming Language :: Python :: 3.7",
        "Programming Language :: Python :: 3.8",
        "Programming Language :: Python :: 3.9",
        "Programming Language :: Python :: 3.10",
        "Programming Language :: Python :: 3.11",
    ],
    python_requires=">=3.7",
    install_requires=[
        "redis>=4.0.0",
    ],
    extras_require={
        "toon": ["python-toon>=1.0.0"],
        "dev": [
            "pytest>=7.0.0",
            "pytest-cov>=3.0.0",
            "black>=22.0.0",
            "flake8>=4.0.0",
        ],
    },
    keywords="redis toon llm token-optimization json",
    project_urls={
        "Bug Reports": "https://github.com/DreamStack-us/redisTOON/issues",
        "Source": "https://github.com/DreamStack-us/redisTOON",
        "Documentation": "https://github.com/DreamStack-us/redisTOON#readme",
    },
)
