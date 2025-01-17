@echo off
REM Create virtual environment if it doesn't exist
if not exist .venv (
    echo Creating virtual environment...
    python -m venv .venv
)

REM Activate the virtual environment
echo Activating virtual environment...
call .\.venv\Scripts\activate

REM Install dependencies
echo Installing dependencies...
pip install -r requirements.txt

REM Build the project with optimization
echo Running build script...
set CFLAGS=-O3
call build.sh

REM Deactivate the virtual environment (optional)
echo Deactivating virtual environment...
deactivate
