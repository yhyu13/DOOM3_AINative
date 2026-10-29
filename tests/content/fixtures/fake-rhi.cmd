@echo off
rem Stub renderer for the capture-harness test (spec 009, T-050 harness). It is NOT a renderer and its
rem output is never evidence: it exists so the capture contract can be tested without a Q4 build.
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0fake-rhi.ps1" %*
