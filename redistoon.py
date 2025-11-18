"""
redisTOON Python Client Library

A Python client for interacting with redisTOON Redis module.
Provides high-level interface for TOON data operations.
"""

import json
from typing import Any, Optional, Union
import redis


class RedisTOON:
    """
    Python client for redisTOON Redis module.

    Example:
        >>> from redistoon import RedisTOON
        >>> r = RedisTOON()
        >>> r.set('mykey', {'name': 'Alice', 'age': 30})
        >>> r.get('mykey')
        {'name': 'Alice', 'age': 30}
    """

    def __init__(self, host='localhost', port=6379, db=0, **kwargs):
        """
        Initialize redisTOON client.

        Args:
            host: Redis server hostname
            port: Redis server port
            db: Redis database number
            **kwargs: Additional arguments passed to redis.Redis()
        """
        self.redis = redis.Redis(host=host, port=port, db=db, **kwargs)

    def set(self, key: str, value: Union[dict, list, str], path: str = '$') -> bool:
        """
        Set TOON data at key.

        Args:
            key: Redis key name
            value: Python object to store (will be converted to TOON)
            path: JSONPath to set (default: '$' for root)

        Returns:
            True if successful

        Example:
            >>> r.set('user:1', {'name': 'Alice', 'age': 30})
            True
        """
        # Import toon encoder if available
        try:
            from toon import encode
            toon_str = encode(value)
        except ImportError:
            # Fallback: use JSON and let Redis convert
            json_str = json.dumps(value)
            result = self.redis.execute_command('TOON.FROMJSON', key, json_str)
            return result == b'OK'

        result = self.redis.execute_command('TOON.SET', key, path, toon_str)
        return result == b'OK'

    def get(self, key: str, path: str = '$') -> Optional[Any]:
        """
        Get TOON data from key.

        Args:
            key: Redis key name
            path: JSONPath to retrieve (default: '$' for root)

        Returns:
            Python object (dict, list, str, etc.) or None if not found

        Example:
            >>> r.get('user:1')
            {'name': 'Alice', 'age': 30}
        """
        toon_str = self.redis.execute_command('TOON.GET', key, path)

        if toon_str is None:
            return None

        # Try to decode TOON format
        try:
            from toon import decode
            return decode(toon_str.decode('utf-8'))
        except ImportError:
            # Fallback: get as JSON
            json_str = self.redis.execute_command('TOON.TOJSON', key, path)
            if json_str:
                return json.loads(json_str)
            return None

    def delete(self, key: str, path: str) -> int:
        """
        Delete value at path.

        Args:
            key: Redis key name
            path: JSONPath to delete

        Returns:
            Number of values deleted (0 or 1)

        Example:
            >>> r.delete('user:1', '$.age')
            1
        """
        return self.redis.execute_command('TOON.DEL', key, path)

    def type(self, key: str, path: str = '$') -> Optional[str]:
        """
        Get type of value at path.

        Args:
            key: Redis key name
            path: JSONPath to check

        Returns:
            Type string ('null', 'boolean', 'number', 'string', 'array', 'object', 'tabular_array')

        Example:
            >>> r.type('user:1', '$.name')
            'string'
        """
        result = self.redis.execute_command('TOON.TYPE', key, path)
        return result.decode('utf-8') if result else None

    def to_json(self, key: str, path: str = '$') -> Optional[str]:
        """
        Convert TOON data to JSON string.

        Args:
            key: Redis key name
            path: JSONPath to convert

        Returns:
            JSON string or None

        Example:
            >>> r.to_json('user:1')
            '{"name":"Alice","age":30}'
        """
        result = self.redis.execute_command('TOON.TOJSON', key, path)
        return result.decode('utf-8') if result else None

    def from_json(self, key: str, json_data: Union[str, dict, list]) -> bool:
        """
        Store JSON data as TOON.

        Args:
            key: Redis key name
            json_data: JSON string or Python object

        Returns:
            True if successful

        Example:
            >>> r.from_json('user:1', '{"name":"Alice","age":30}')
            True
        """
        if not isinstance(json_data, str):
            json_data = json.dumps(json_data)

        result = self.redis.execute_command('TOON.FROMJSON', key, json_data)
        return result == b'OK'

    def token_count(self, key: str, path: str = '$') -> int:
        """
        Estimate token count for TOON data.

        Args:
            key: Redis key name
            path: JSONPath to count

        Returns:
            Approximate token count

        Example:
            >>> r.token_count('user:1')
            15
        """
        return self.redis.execute_command('TOON.TOKENCOUNT', key, path)

    def array_append(self, key: str, path: str, *values) -> int:
        """
        Append one or more values to an array.

        Args:
            key: Redis key name
            path: JSONPath to array
            *values: Values to append (can be Python objects)

        Returns:
            New length of the array

        Example:
            >>> r.array_append('doc', '$.tags', 'new-tag', 'another-tag')
            5
        """
        try:
            from toon import encode
            toon_values = [encode(v) if not isinstance(v, str) else v for v in values]
        except ImportError:
            # Fallback: use JSON encoding
            toon_values = [json.dumps(v) if not isinstance(v, str) else v for v in values]

        return self.redis.execute_command('TOON.ARRAPPEND', key, path, *toon_values)

    def array_insert(self, key: str, path: str, index: int, value: Any) -> int:
        """
        Insert a value into an array at a specific index.

        Args:
            key: Redis key name
            path: JSONPath to array
            index: Index to insert at (can be negative)
            value: Value to insert

        Returns:
            New length of the array

        Example:
            >>> r.array_insert('doc', '$.tags', 0, 'first-tag')
            4
        """
        try:
            from toon import encode
            toon_value = encode(value) if not isinstance(value, str) else value
        except ImportError:
            toon_value = json.dumps(value) if not isinstance(value, str) else value

        return self.redis.execute_command('TOON.ARRINSERT', key, path, index, toon_value)

    def array_pop(self, key: str, path: str, index: int = -1) -> Optional[Any]:
        """
        Pop (remove and return) a value from an array.

        Args:
            key: Redis key name
            path: JSONPath to array
            index: Index to pop (default: -1 for last element)

        Returns:
            The popped value or None

        Example:
            >>> r.array_pop('doc', '$.tags')
            'last-tag'
        """
        toon_str = self.redis.execute_command('TOON.ARRPOP', key, path, index)

        if toon_str is None:
            return None

        try:
            from toon import decode
            return decode(toon_str.decode('utf-8'))
        except ImportError:
            # If it looks like JSON, parse it
            try:
                return json.loads(toon_str.decode('utf-8'))
            except:
                return toon_str.decode('utf-8')

    def array_length(self, key: str, path: str) -> Optional[int]:
        """
        Get the length of an array.

        Args:
            key: Redis key name
            path: JSONPath to array

        Returns:
            Length of the array or None

        Example:
            >>> r.array_length('doc', '$.tags')
            3
        """
        return self.redis.execute_command('TOON.ARRLEN', key, path)

    def merge(self, key: str, path: str, value: dict) -> bool:
        """
        Merge a TOON object with another object.

        Args:
            key: Redis key name
            path: JSONPath to object
            value: Object to merge (will be deep merged)

        Returns:
            True if successful

        Example:
            >>> r.merge('user:1', '$', {'extra': 'data', 'age': 31})
            True
        """
        try:
            from toon import encode
            toon_str = encode(value)
        except ImportError:
            # Fallback: convert through JSON
            json_str = json.dumps(value)
            temp_key = '__temp_merge__'
            self.from_json(temp_key, value)
            toon_str = self.redis.execute_command('TOON.GET', temp_key)
            self.redis.delete(temp_key)

        result = self.redis.execute_command('TOON.MERGE', key, path, toon_str)
        return result == b'OK'

    def validate(self, key: str) -> bool:
        """
        Validate TOON document structure.

        Args:
            key: Redis key name

        Returns:
            True if valid, False otherwise

        Example:
            >>> r.validate('user:1')
            True
        """
        try:
            result = self.redis.execute_command('TOON.VALIDATE', key)
            return result == b'OK'
        except:
            return False

    def compare_efficiency(self, data: dict) -> dict:
        """
        Compare token efficiency between JSON and TOON formats.

        Args:
            data: Python dictionary to compare

        Returns:
            Dictionary with JSON tokens, TOON tokens, and reduction percentage

        Example:
            >>> data = {'users': [{'id': 1, 'name': 'Alice'}, {'id': 2, 'name': 'Bob'}]}
            >>> r.compare_efficiency(data)
            {'json_tokens': 45, 'toon_tokens': 28, 'reduction': 38}
        """
        # Store as JSON first
        json_str = json.dumps(data)
        temp_key = '__temp_comparison__'

        # Get JSON token count (rough estimate)
        json_tokens = len(json_str.split())  # Rough approximation

        # Store as TOON
        self.from_json(temp_key, data)
        toon_tokens = self.token_count(temp_key)

        # Clean up
        self.redis.delete(temp_key)

        reduction = int((1 - toon_tokens / json_tokens) * 100) if json_tokens > 0 else 0

        return {
            'json_tokens': json_tokens,
            'toon_tokens': toon_tokens,
            'reduction': reduction
        }


# Convenience function
def create_client(host='localhost', port=6379, db=0, **kwargs) -> RedisTOON:
    """
    Create a redisTOON client instance.

    Args:
        host: Redis server hostname
        port: Redis server port
        db: Redis database number
        **kwargs: Additional arguments passed to redis.Redis()

    Returns:
        RedisTOON client instance
    """
    return RedisTOON(host=host, port=port, db=db, **kwargs)


if __name__ == '__main__':
    # Example usage
    print("redisTOON Python Client Example")
    print("=" * 50)

    # Create client
    r = RedisTOON()

    # Example 1: Simple object
    print("\n1. Storing simple object:")
    data = {
        'name': 'Alice',
        'age': 30,
        'role': 'admin'
    }
    r.set('user:1', data)
    print(f"Stored: {data}")
    print(f"Retrieved: {r.get('user:1')}")

    # Example 2: Array of objects (will be stored as tabular array)
    print("\n2. Storing array of objects (tabular format):")
    users = [
        {'id': 1, 'name': 'Alice', 'role': 'admin'},
        {'id': 2, 'name': 'Bob', 'role': 'user'},
        {'id': 3, 'name': 'Charlie', 'role': 'moderator'}
    ]
    r.set('users', users)
    print(f"Stored {len(users)} users")
    print(f"Token count: {r.token_count('users')}")

    # Example 3: Efficiency comparison
    print("\n3. Token efficiency comparison:")
    efficiency = r.compare_efficiency({'users': users})
    print(f"JSON tokens: {efficiency['json_tokens']}")
    print(f"TOON tokens: {efficiency['toon_tokens']}")
    print(f"Reduction: {efficiency['reduction']}%")

    print("\n" + "=" * 50)
    print("Examples complete!")
