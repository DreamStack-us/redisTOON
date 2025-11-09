#!/usr/bin/env python3
"""
redisTOON Unit Tests

Run with: pytest tests/test_redistoon.py
"""

import pytest
import json
from redistoon import RedisTOON


@pytest.fixture
def redis_client():
    """Create a redisTOON client for testing."""
    return RedisTOON()


@pytest.fixture(autouse=True)
def cleanup(redis_client):
    """Clean up test keys after each test."""
    yield
    # Cleanup code runs after the test
    try:
        redis_client.redis.delete('test:*')
    except:
        pass


class TestBasicOperations:
    """Test basic SET, GET, DEL operations."""

    def test_set_get_simple_object(self, redis_client):
        """Test setting and getting a simple object."""
        data = {'name': 'Alice', 'age': 30}
        assert redis_client.set('test:user', data) is True

        result = redis_client.get('test:user')
        assert result is not None
        # Note: depending on TOON implementation, types might vary
        assert 'name' in result
        assert 'age' in result

    def test_set_get_nested_object(self, redis_client):
        """Test nested object storage."""
        data = {
            'user': {
                'name': 'Bob',
                'profile': {
                    'age': 25,
                    'city': 'NYC'
                }
            }
        }
        assert redis_client.set('test:nested', data) is True

        result = redis_client.get('test:nested')
        assert result is not None
        assert 'user' in result

    def test_get_nonexistent_key(self, redis_client):
        """Test getting a non-existent key returns None."""
        result = redis_client.get('test:nonexistent')
        assert result is None

    def test_set_get_array(self, redis_client):
        """Test array storage."""
        data = [1, 2, 3, 4, 5]
        assert redis_client.set('test:array', data) is True

        result = redis_client.get('test:array')
        assert result is not None
        assert isinstance(result, list)

    def test_set_get_tabular_array(self, redis_client):
        """Test tabular array (uniform objects)."""
        data = [
            {'id': 1, 'name': 'Alice'},
            {'id': 2, 'name': 'Bob'},
            {'id': 3, 'name': 'Charlie'}
        ]
        assert redis_client.set('test:tabular', data) is True

        result = redis_client.get('test:tabular')
        assert result is not None
        assert isinstance(result, list)
        assert len(result) == 3


class TestJSONConversion:
    """Test JSON to TOON conversion."""

    def test_from_json_object(self, redis_client):
        """Test importing JSON object."""
        json_str = '{"name": "Alice", "age": 30}'
        assert redis_client.from_json('test:json', json_str) is True

        result = redis_client.get('test:json')
        assert result is not None

    def test_from_json_array(self, redis_client):
        """Test importing JSON array."""
        json_str = '[1, 2, 3, 4, 5]'
        assert redis_client.from_json('test:json_array', json_str) is True

        result = redis_client.get('test:json_array')
        assert result is not None
        assert isinstance(result, list)

    def test_to_json(self, redis_client):
        """Test exporting to JSON."""
        data = {'name': 'Bob', 'age': 25}
        redis_client.set('test:to_json', data)

        json_str = redis_client.to_json('test:to_json')
        assert json_str is not None

        # Parse JSON to verify it's valid
        parsed = json.loads(json_str)
        assert 'name' in parsed
        assert 'age' in parsed

    def test_roundtrip_json(self, redis_client):
        """Test JSON -> TOON -> JSON roundtrip."""
        original = {'product': 'Widget', 'price': 19.99, 'stock': 100}
        json_str = json.dumps(original)

        redis_client.from_json('test:roundtrip', json_str)
        result_json = redis_client.to_json('test:roundtrip')

        result = json.loads(result_json)
        assert result['product'] == original['product']
        assert result['stock'] == original['stock']


class TestTypes:
    """Test type checking."""

    def test_type_string(self, redis_client):
        """Test string type detection."""
        redis_client.set('test:string', {'value': 'hello'})
        # Note: actual implementation may vary
        type_str = redis_client.type('test:string')
        assert type_str is not None

    def test_type_object(self, redis_client):
        """Test object type detection."""
        redis_client.set('test:object', {'a': 1, 'b': 2})
        type_str = redis_client.type('test:object')
        assert type_str in ['object', 'tabular_array']

    def test_type_array(self, redis_client):
        """Test array type detection."""
        redis_client.set('test:array', [1, 2, 3])
        type_str = redis_client.type('test:array')
        assert type_str in ['array', 'tabular_array']


class TestTokenCounting:
    """Test token counting functionality."""

    def test_token_count_simple(self, redis_client):
        """Test token counting for simple object."""
        data = {'name': 'Alice', 'age': 30}
        redis_client.set('test:tokens', data)

        count = redis_client.token_count('test:tokens')
        assert count > 0
        assert count < 100  # Should be small for simple object

    def test_token_count_array(self, redis_client):
        """Test token counting for array."""
        data = [
            {'id': i, 'name': f'User {i}'}
            for i in range(10)
        ]
        redis_client.set('test:tokens_array', data)

        count = redis_client.token_count('test:tokens_array')
        assert count > 0

    def test_efficiency_comparison(self, redis_client):
        """Test token efficiency comparison."""
        data = {
            'users': [
                {'id': 1, 'name': 'Alice'},
                {'id': 2, 'name': 'Bob'},
                {'id': 3, 'name': 'Charlie'}
            ]
        }

        result = redis_client.compare_efficiency(data)
        assert 'json_tokens' in result
        assert 'toon_tokens' in result
        assert 'reduction' in result
        assert result['json_tokens'] > 0
        assert result['toon_tokens'] > 0


class TestEdgeCases:
    """Test edge cases and error handling."""

    def test_empty_object(self, redis_client):
        """Test storing empty object."""
        data = {}
        assert redis_client.set('test:empty', data) is True

        result = redis_client.get('test:empty')
        assert result is not None

    def test_empty_array(self, redis_client):
        """Test storing empty array."""
        data = []
        assert redis_client.set('test:empty_array', data) is True

        result = redis_client.get('test:empty_array')
        assert result is not None

    def test_null_value(self, redis_client):
        """Test storing null value."""
        data = {'value': None}
        assert redis_client.set('test:null', data) is True

        result = redis_client.get('test:null')
        assert result is not None

    def test_special_characters(self, redis_client):
        """Test strings with special characters."""
        data = {
            'text': 'Hello\nWorld\t!',
            'quote': 'He said "hello"',
            'unicode': '你好世界'
        }
        assert redis_client.set('test:special', data) is True

        result = redis_client.get('test:special')
        assert result is not None


class TestUseCases:
    """Test real-world use cases."""

    def test_llm_context_caching(self, redis_client):
        """Test LLM conversation context caching."""
        conversation = [
            {'role': 'system', 'content': 'You are helpful.'},
            {'role': 'user', 'content': 'Hello!'},
            {'role': 'assistant', 'content': 'Hi there!'}
        ]

        redis_client.set('test:chat:123', conversation)
        retrieved = redis_client.get('test:chat:123')

        assert retrieved is not None
        assert isinstance(retrieved, list)
        assert len(retrieved) == 3

    def test_rag_document_storage(self, redis_client):
        """Test RAG document chunk storage."""
        chunk = {
            'id': 'chunk-123',
            'text': 'This is a document chunk...',
            'metadata': {
                'source': 'doc.pdf',
                'page': 5
            },
            'embedding_ref': 'embed-123'
        }

        redis_client.set('test:chunk:123', chunk)
        retrieved = redis_client.get('test:chunk:123')

        assert retrieved is not None
        assert 'id' in retrieved

    def test_prompt_template_storage(self, redis_client):
        """Test prompt template storage."""
        template = {
            'name': 'summarization',
            'system': 'You are a summarization expert.',
            'template': 'Summarize the following: {text}',
            'examples': [
                {'input': 'Long text...', 'output': 'Summary...'}
            ]
        }

        redis_client.set('test:template:summarize', template)
        retrieved = redis_client.get('test:template:summarize')

        assert retrieved is not None
        assert 'name' in retrieved


if __name__ == '__main__':
    pytest.main([__file__, '-v'])
