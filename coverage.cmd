@echo off

OpenCppCoverage --sources "libaniparse" ^
--excluded_sources "lexbor" ^
--excluded_sources "libasyncnet" ^
--excluded_sources "catch_amalgamated*" ^
--export_type "html:CoverageReport" ^
-- "out\build\x64-debug\tests\aniparse-tests"

start CoverageReport\index.html