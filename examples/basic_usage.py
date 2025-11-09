#!/usr/bin/env python3
"""
Basic redisTOON Usage Examples

This script demonstrates the core functionality of redisTOON.
"""

from redistoon import RedisTOON
import json


def main():
    print("=" * 60)
    print("redisTOON Basic Usage Examples")
    print("=" * 60)

    # Initialize client
    r = RedisTOON()

    # Example 1: Simple object storage
    print("\n1. Simple Object Storage")
    print("-" * 60)

    user = {
        'id': 1,
        'name': 'Alice Johnson',
        'email': 'alice@example.com',
        'role': 'admin',
        'active': True
    }

    r.set('user:1', user)
    retrieved = r.get('user:1')

    print(f"Original:  {user}")
    print(f"Retrieved: {retrieved}")
    print(f"Match: {user == retrieved}")

    # Example 2: Nested objects
    print("\n2. Nested Objects")
    print("-" * 60)

    config = {
        'app': {
            'name': 'MyApp',
            'version': '1.0.0',
            'debug': False
        },
        'database': {
            'host': 'localhost',
            'port': 5432,
            'name': 'mydb'
        }
    }

    r.set('config:app', config)
    print(f"Stored configuration: {r.get('config:app')}")

    # Example 3: Arrays (will be stored as tabular arrays if uniform)
    print("\n3. Tabular Arrays (Uniform Objects)")
    print("-" * 60)

    users = [
        {'id': 1, 'name': 'Alice', 'role': 'admin'},
        {'id': 2, 'name': 'Bob', 'role': 'user'},
        {'id': 3, 'name': 'Charlie', 'role': 'moderator'},
        {'id': 4, 'name': 'Diana', 'role': 'user'},
        {'id': 5, 'name': 'Eve', 'role': 'admin'}
    ]

    r.set('users:list', users)
    print(f"Stored {len(users)} users")
    print(f"Retrieved: {r.get('users:list')}")

    # Check token efficiency
    tokens = r.token_count('users:list')
    print(f"Token count: {tokens}")

    # Example 4: Type checking
    print("\n4. Type Checking")
    print("-" * 60)

    r.set('test:types', {
        'string_val': 'hello',
        'number_val': 42,
        'bool_val': True,
        'null_val': None,
        'array_val': [1, 2, 3]
    })

    print(f"Root type: {r.type('test:types')}")

    # Example 5: JSON conversion
    print("\n5. JSON Conversion")
    print("-" * 60)

    json_data = '{"product": "Widget", "price": 19.99, "stock": 100}'
    r.from_json('product:1', json_data)

    print(f"Original JSON: {json_data}")
    print(f"Retrieved as Python: {r.get('product:1')}")
    print(f"Back to JSON: {r.to_json('product:1')}")

    # Example 6: Token efficiency comparison
    print("\n6. Token Efficiency Comparison")
    print("-" * 60)

    large_dataset = {
        'records': [
            {
                'id': i,
                'name': f'User {i}',
                'email': f'user{i}@example.com',
                'status': 'active' if i % 2 == 0 else 'inactive'
            }
            for i in range(1, 21)
        ]
    }

    efficiency = r.compare_efficiency(large_dataset)
    print(f"Dataset: 20 user records")
    print(f"JSON tokens (approx): {efficiency['json_tokens']}")
    print(f"TOON tokens: {efficiency['toon_tokens']}")
    print(f"Token reduction: {efficiency['reduction']}%")
    print(f"Savings: {efficiency['json_tokens'] - efficiency['toon_tokens']} tokens")

    # Example 7: LLM Context Caching
    print("\n7. LLM Context Caching")
    print("-" * 60)

    conversation = [
        {'role': 'system', 'content': 'You are a helpful assistant.'},
        {'role': 'user', 'content': 'What is Redis?'},
        {'role': 'assistant', 'content': 'Redis is an in-memory data structure store.'},
        {'role': 'user', 'content': 'What about TOON?'},
        {'role': 'assistant', 'content': 'TOON is a token-efficient format for LLMs.'}
    ]

    r.set('chat:session:123', conversation)
    tokens = r.token_count('chat:session:123')

    print(f"Conversation: {len(conversation)} messages")
    print(f"Token count: {tokens}")
    print("Perfect for caching LLM context!")

    print("\n" + "=" * 60)
    print("Examples completed successfully!")
    print("=" * 60)


if __name__ == '__main__':
    try:
        main()
    except Exception as e:
        print(f"\nError: {e}")
        print("\nMake sure:")
        print("1. Redis is running")
        print("2. redisTOON module is loaded")
        print("3. Redis is accessible at localhost:6379")
