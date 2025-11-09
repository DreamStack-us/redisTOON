# Contributing to redisTOON

Thank you for your interest in contributing to redisTOON! We welcome contributions from the community.

## How to Contribute

### Reporting Issues

If you find a bug or have a feature request, please:

1. Check if the issue already exists in [GitHub Issues](https://github.com/DreamStack-us/redisTOON/issues)
2. If not, create a new issue with:
   - Clear description of the problem or feature
   - Steps to reproduce (for bugs)
   - Expected vs actual behavior
   - Your environment (OS, Redis version, etc.)

### Submitting Pull Requests

1. **Fork the repository** and create your branch from `main`

   ```bash
   git checkout -b feature/my-new-feature
   ```

2. **Make your changes**
   - Follow the existing code style
   - Add tests for new functionality
   - Update documentation as needed

3. **Test your changes**

   ```bash
   # Build the module
   mkdir build && cd build
   cmake ..
   make

   # Run tests
   cd ../tests
   python -m pytest
   ```

4. **Commit your changes**
   - Use clear, descriptive commit messages
   - Follow conventional commit format when possible:
     ```
     feat: Add new TOON.MERGE command
     fix: Resolve memory leak in decoder
     docs: Update installation instructions
     ```

5. **Push to your fork** and submit a pull request

   ```bash
   git push origin feature/my-new-feature
   ```

6. **Wait for review**
   - Address any feedback from maintainers
   - Keep your PR focused on a single feature or fix

## Development Setup

### Prerequisites

- GCC or Clang compiler
- CMake 3.10+
- Redis 7.0+
- Python 3.7+ (for client library)

### Building from Source

```bash
# Clone your fork
git clone https://github.com/YOUR_USERNAME/redisTOON.git
cd redisTOON

# Build the module
mkdir build && cd build
cmake -DCMAKE_BUILD_TYPE=Debug ..
make

# Run Redis with the module
redis-server --loadmodule ./redistoon.so
```

### Running Tests

```bash
# Unit tests
cd tests/unit
python -m pytest -v

# Integration tests
cd tests/integration
python -m pytest -v

# All tests
cd tests
python -m pytest -v
```

## Code Style Guidelines

### C Code

- Use K&R style indentation (4 spaces)
- Function names use `snake_case`
- Type names use `PascalCase`
- Constants use `UPPER_CASE`
- Add comments for complex logic
- Keep functions focused and small

Example:
```c
// Good
ToonValue *toon_value_create(ToonType type) {
    ToonValue *value = calloc(1, sizeof(ToonValue));
    if (!value) return NULL;

    value->type = type;
    return value;
}
```

### Python Code

- Follow PEP 8
- Use type hints
- Add docstrings for public APIs
- Use `black` for formatting
- Use `flake8` for linting

Example:
```python
def set(self, key: str, value: dict, path: str = '$') -> bool:
    """
    Set TOON data at key.

    Args:
        key: Redis key name
        value: Python object to store
        path: JSONPath to set

    Returns:
        True if successful
    """
    ...
```

## Areas We Need Help

Here are some areas where we'd especially appreciate contributions:

### High Priority

- [ ] Path query optimization (wildcard support)
- [ ] Array operations (APPEND, INSERT, POP)
- [ ] TOON spec compliance testing
- [ ] Performance benchmarking

### Medium Priority

- [ ] Client libraries (Node.js, Go, Java)
- [ ] Integration with LangChain/LlamaIndex
- [ ] Docker container setup
- [ ] CI/CD pipeline (GitHub Actions)

### Documentation

- [ ] API reference documentation
- [ ] Tutorial examples
- [ ] Use case guides
- [ ] Performance tuning guide

## Testing Requirements

All code contributions should include:

1. **Unit tests** - Test individual functions/components
2. **Integration tests** - Test end-to-end workflows
3. **Documentation** - Update README or docs as needed

### Writing Tests

Tests use pytest:

```python
def test_toon_set_get():
    """Test basic SET and GET operations."""
    r = RedisTOON()
    data = {'name': 'Alice', 'age': 30}

    assert r.set('test:key', data) is True
    result = r.get('test:key')

    assert result == data
```

## Documentation

When adding new features:

1. Update the README.md if needed
2. Add docstrings to all public APIs
3. Create examples in the `examples/` directory
4. Update the command reference table

## Community Guidelines

- Be respectful and inclusive
- Help others learn and grow
- Give constructive feedback
- Follow our [Code of Conduct](CODE_OF_CONDUCT.md)

## Questions?

- Open a [Discussion](https://github.com/DreamStack-us/redisTOON/discussions)
- Join our community channels
- Reach out to maintainers

## License

By contributing, you agree that your contributions will be licensed under the MIT License.

---

Thank you for contributing to redisTOON! 🎉
