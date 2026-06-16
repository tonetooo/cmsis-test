"""
Memory API routes for the agent memory system.
Provides endpoints to add, search, update, and delete memories.
"""

from flask import Blueprint, request, jsonify
from agent_memory import MemoryService

memory_bp = Blueprint("memory", __name__, url_prefix="/memory")


@memory_bp.route("", methods=["POST"])
def add_memory():
    """Add a new memory entry."""
    data = request.get_json()
    if not data or "content" not in data:
        return jsonify({"error": "Content is required"}), 400

    memory = MemoryService.get_instance()
    entry = memory.add(
        content=data["content"],
        category=data.get("category", "development"),
        tags=data.get("tags", []),
        importance=data.get("importance", 1.0),
        source=data.get("source", "agent"),
    )
    return jsonify({"message": "Memory added", "entry": entry.to_dict()}), 201


@memory_bp.route("", methods=["GET"])
def search_memory():
    """Search memories by query, category, or tags."""
    memory = MemoryService.get_instance()
    results = memory.search(
        query=request.args.get("query"),
        category=request.args.get("category"),
        tags=request.args.getlist("tags"),
        top_k=int(request.args.get("top_k", 5)),
    )
    return jsonify({
        "count": len(results),
        "memories": [m.to_dict() for m in results],
    })


@memory_bp.route("/<memory_id>", methods=["GET"])
def get_memory(memory_id):
    """Get a specific memory by ID."""
    memory = MemoryService.get_instance()
    entry = memory.get_by_id(memory_id)
    if entry:
        return jsonify(entry.to_dict())
    return jsonify({"error": "Memory not found"}), 404


@memory_bp.route("/<memory_id>", methods=["PUT"])
def update_memory(memory_id):
    """Update an existing memory."""
    data = request.get_json()
    if not data:
        return jsonify({"error": "Request body is required"}), 400

    memory = MemoryService.get_instance()
    entry = memory.update(
        memory_id=memory_id,
        content=data.get("content"),
        importance=data.get("importance"),
        tags=data.get("tags"),
    )
    if entry:
        return jsonify({"message": "Memory updated", "entry": entry.to_dict()})
    return jsonify({"error": "Memory not found"}), 404


@memory_bp.route("/<memory_id>", methods=["DELETE"])
def delete_memory(memory_id):
    """Delete a memory."""
    memory = MemoryService.get_instance()
    if memory.delete(memory_id):
        return jsonify({"message": "Memory deleted"})
    return jsonify({"error": "Memory not found"}), 404


@memory_bp.route("/summary", methods=["GET"])
def memory_summary():
    """Get a summary of all memories."""
    memory = MemoryService.get_instance()
    return jsonify(memory.get_summary())


@memory_bp.route("/recent", methods=["GET"])
def recent_memories():
    """Get the most recent memories."""
    limit = int(request.args.get("limit", 10))
    memory = MemoryService.get_instance()
    results = memory.get_recent(limit=limit)
    return jsonify({
        "count": len(results),
        "memories": [m.to_dict() for m in results],
    })


@memory_bp.route("/context", methods=["GET"])
def memory_context():
    """Get formatted context for LLM prompts."""
    query = request.args.get("query", "")
    max_memories = int(request.args.get("max_memories", 3))
    memory = MemoryService.get_instance()
    context = memory.get_context_for_prompt(query, max_memories)
    return jsonify({"context": context})


@memory_bp.route("/clear", methods=["POST"])
def clear_memories():
    """Clear all memories (use with caution)."""
    memory = MemoryService.get_instance()
    memory.clear()
    return jsonify({"message": "All memories cleared"})
