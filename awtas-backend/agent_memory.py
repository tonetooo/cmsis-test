"""
Agent Memory System for AWTAS Backend
Persistent memory layer that remembers project developments, decisions, and context.
Based on Mem0-inspired architecture with selective extraction and multi-signal retrieval.
"""

import json
import os
import hashlib
from datetime import datetime
from typing import Optional
from dataclasses import dataclass, field, asdict
from pathlib import Path


@dataclass
class MemoryEntry:
    """Single memory entry with metadata."""
    id: str
    content: str
    category: str
    timestamp: str
    tags: list = field(default_factory=list)
    importance: float = 1.0
    source: str = "agent"
    last_accessed: Optional[str] = None
    access_count: int = 0

    def to_dict(self):
        return asdict(self)

    @classmethod
    def from_dict(cls, data):
        return cls(**{k: v for k, v in data.items() if k in cls.__dataclass_fields__})


class AgentMemory:
    """
    Persistent memory system for the AWTAS backend agent.
    Stores and retrieves project developments, decisions, and context.
    """

    CATEGORIES = {
        "architecture": "Architectural decisions and patterns",
        "development": "Development progress and features",
        "bugfix": "Bug fixes and troubleshooting",
        "configuration": "Configuration changes and settings",
        "deployment": "Deployment and infrastructure",
        "api": "API changes and endpoints",
        "context": "Project context and requirements",
        "learnings": "Lessons learned and best practices",
    }

    def __init__(self, storage_path: Optional[str] = None):
        if storage_path is None:
            storage_path = os.path.join(
                os.path.dirname(os.path.abspath(__file__)),
                "agent_memory.json"
            )
        self.storage_path = storage_path
        self.memories: list[MemoryEntry] = []
        self._load()

    def _load(self):
        """Load memories from persistent storage."""
        if os.path.exists(self.storage_path):
            try:
                with open(self.storage_path, "r", encoding="utf-8") as f:
                    data = json.load(f)
                    self.memories = [
                        MemoryEntry.from_dict(m) for m in data.get("memories", [])
                    ]
            except (json.JSONDecodeError, KeyError):
                self.memories = []

    def _save(self):
        """Persist memories to storage."""
        os.makedirs(os.path.dirname(self.storage_path), exist_ok=True)
        with open(self.storage_path, "w", encoding="utf-8") as f:
            json.dump({
                "memories": [m.to_dict() for m in self.memories],
                "last_updated": datetime.now().isoformat(),
                "total_memories": len(self.memories),
            }, f, indent=2, ensure_ascii=False)

    def _generate_id(self, content: str) -> str:
        """Generate a unique ID for a memory entry."""
        timestamp = datetime.now().isoformat()
        return hashlib.md5(f"{content}{timestamp}".encode()).hexdigest()[:12]

    def add(
        self,
        content: str,
        category: str = "development",
        tags: Optional[list] = None,
        importance: float = 1.0,
        source: str = "agent",
    ) -> MemoryEntry:
        """
        Add a new memory entry.

        Args:
            content: The memory content (development info, decision, etc.)
            category: Category from CATEGORIES
            tags: Optional tags for filtering
            importance: Importance score (0.1 to 1.0)
            source: Source of the memory (agent, user, system)
        """
        if category not in self.CATEGORIES:
            raise ValueError(f"Invalid category. Choose from: {list(self.CATEGORIES.keys())}")

        entry = MemoryEntry(
            id=self._generate_id(content),
            content=content,
            category=category,
            timestamp=datetime.now().isoformat(),
            tags=tags or [],
            importance=max(0.1, min(1.0, importance)),
            source=source,
        )

        self.memories.append(entry)
        self._save()
        return entry

    def search(
        self,
        query: Optional[str] = None,
        category: Optional[str] = None,
        tags: Optional[list] = None,
        top_k: int = 5,
    ) -> list[MemoryEntry]:
        """
        Search memories by query, category, or tags.

        Args:
            query: Text to search in content (keyword match)
            category: Filter by category
            tags: Filter by tags (must match all)
            top_k: Maximum number of results
        """
        results = self.memories.copy()

        if category:
            results = [m for m in results if m.category == category]

        if tags:
            results = [m for m in results if all(t in m.tags for t in tags)]

        if query:
            query_lower = query.lower()
            scored = []
            for m in results:
                content_lower = m.content.lower()
                score = 0
                if query_lower in content_lower:
                    score += 10
                for word in query_lower.split():
                    if word in content_lower:
                        score += 1
                for tag in m.tags:
                    if query_lower in tag.lower():
                        score += 5
                scored.append((score * m.importance, m))

            scored.sort(key=lambda x: x[0], reverse=True)
            results = [m for _, m in scored]

        results.sort(key=lambda m: m.timestamp, reverse=True)

        for m in results[:top_k]:
            m.last_accessed = datetime.now().isoformat()
            m.access_count += 1

        self._save()
        return results[:top_k]

    def get_by_id(self, memory_id: str) -> Optional[MemoryEntry]:
        """Retrieve a specific memory by ID."""
        for m in self.memories:
            if m.id == memory_id:
                m.last_accessed = datetime.now().isoformat()
                m.access_count += 1
                self._save()
                return m
        return None

    def update(
        self,
        memory_id: str,
        content: Optional[str] = None,
        importance: Optional[float] = None,
        tags: Optional[list] = None,
    ) -> Optional[MemoryEntry]:
        """Update an existing memory entry."""
        for i, m in enumerate(self.memories):
            if m.id == memory_id:
                if content is not None:
                    self.memories[i].content = content
                if importance is not None:
                    self.memories[i].importance = max(0.1, min(1.0, importance))
                if tags is not None:
                    self.memories[i].tags = tags
                self._save()
                return self.memories[i]
        return None

    def delete(self, memory_id: str) -> bool:
        """Delete a memory entry."""
        for i, m in enumerate(self.memories):
            if m.id == memory_id:
                self.memories.pop(i)
                self._save()
                return True
        return False

    def get_summary(self) -> dict:
        """Get a summary of all memories by category."""
        summary = {cat: 0 for cat in self.CATEGORIES}
        for m in self.memories:
            summary[m.category] = summary.get(m.category, 0) + 1

        return {
            "total_memories": len(self.memories),
            "by_category": summary,
            "last_updated": max(
                [m.timestamp for m in self.memories], default=None
            ),
        }

    def get_recent(self, limit: int = 10) -> list[MemoryEntry]:
        """Get the most recent memories."""
        sorted_memories = sorted(
            self.memories, key=lambda m: m.timestamp, reverse=True
        )
        return sorted_memories[:limit]

    def get_context_for_prompt(self, query: str, max_memories: int = 3) -> str:
        """
        Format memories as context for LLM prompts.
        Returns a formatted string with relevant memories.
        """
        relevant = self.search(query=query, top_k=max_memories)
        if not relevant:
            return "No relevant previous developments found."

        lines = ["Previous project context:"]
        for i, m in enumerate(relevant, 1):
            lines.append(f"{i}. [{m.category.upper()}] {m.content}")
            if m.tags:
                lines.append(f"   Tags: {', '.join(m.tags)}")
        return "\n".join(lines)

    def clear(self):
        """Clear all memories."""
        self.memories = []
        self._save()


class MemoryService:
    """
    Singleton service wrapper for AgentMemory.
    Provides a global access point for the Flask application.
    """
    _instance: Optional[AgentMemory] = None

    @classmethod
    def get_instance(cls, storage_path: Optional[str] = None) -> AgentMemory:
        if cls._instance is None:
            cls._instance = AgentMemory(storage_path=storage_path)
        return cls._instance

    @classmethod
    def reset(cls):
        cls._instance = None
