#!/bin/bash

# Start script for SAT Solver Visualization Server

echo "🚀 Starting SAT Solver Visualization Server..."
echo ""

# Check if we're in the right directory
if [ ! -f "backend/main.py" ]; then
    echo "❌ Error: Please run this script from the visualization_server directory"
    exit 1
fi

# Check if venv exists
if [ ! -d "../venv" ]; then
    echo "❌ Error: Virtual environment not found at ../venv"
    echo "Please create a venv in the bsat root directory first:"
    echo "  cd .."
    echo "  python3 -m venv venv"
    echo "  source venv/bin/activate"
    echo "  pip install -r visualization_server/requirements.txt"
    exit 1
fi

# Activate venv
source ../venv/bin/activate

# Check if dependencies are installed
python3 -c "import fastapi, uvicorn" 2>/dev/null
if [ $? -ne 0 ]; then
    echo "❌ Error: FastAPI dependencies not installed"
    echo "Please install dependencies:"
    echo "  pip install -r requirements.txt"
    exit 1
fi

# Start the server
echo "✓ Dependencies OK"
echo "✓ Starting server on http://localhost:8000"
echo ""
echo "Press Ctrl+C to stop the server"
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo ""

python3 -m uvicorn backend.main:app --host 0.0.0.0 --port 8000 --reload
