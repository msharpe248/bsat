"""Session management for solver visualizations."""

import uuid
from typing import Dict, Any, Optional
from datetime import datetime, timedelta


class SessionManager:
    """Manages active solver visualization sessions."""

    def __init__(self):
        self.sessions: Dict[str, Dict[str, Any]] = {}
        self.max_age = timedelta(hours=1)  # Sessions expire after 1 hour

    def create_session(self, formula: str, algorithm: str, speed: int = 500) -> str:
        """Create a new session and return its ID."""
        session_id = str(uuid.uuid4())
        self.sessions[session_id] = {
            "formula": formula,
            "algorithm": algorithm,
            "speed": speed,
            "created_at": datetime.now(),
            "state_history": [],
            "active": True
        }
        return session_id

    def get_session(self, session_id: str) -> Optional[Dict[str, Any]]:
        """Get session by ID."""
        session = self.sessions.get(session_id)
        if session and self._is_session_valid(session):
            return session
        return None

    def update_session(self, session_id: str, **kwargs):
        """Update session data."""
        if session_id in self.sessions:
            self.sessions[session_id].update(kwargs)

    def add_state(self, session_id: str, state: Dict[str, Any]):
        """Add a state update to session history."""
        if session_id in self.sessions:
            self.sessions[session_id]["state_history"].append(state)

    def close_session(self, session_id: str):
        """Mark session as inactive."""
        if session_id in self.sessions:
            self.sessions[session_id]["active"] = False

    def cleanup_old_sessions(self):
        """Remove expired sessions."""
        current_time = datetime.now()
        expired = [
            sid for sid, session in self.sessions.items()
            if current_time - session["created_at"] > self.max_age
        ]
        for sid in expired:
            del self.sessions[sid]

    def _is_session_valid(self, session: Dict[str, Any]) -> bool:
        """Check if session is still valid."""
        age = datetime.now() - session["created_at"]
        return age <= self.max_age


# Global session manager instance
session_manager = SessionManager()
