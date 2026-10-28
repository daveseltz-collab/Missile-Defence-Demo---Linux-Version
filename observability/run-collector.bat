@echo off
setlocal EnableDelayedExpansion

set "NDDSHOME=C:\RTI\rti_connext_dds-7.7.0"
set "COLLECTOR=%NDDSHOME%\bin\rticollectorservice.bat"

if not exist "%COLLECTOR%" (
    echo ERROR: Collector Service not found: %COLLECTOR%
    echo        Verify RTI Connext 7.7.0 is installed at %NDDSHOME%
    pause
    exit /b 1
)

echo Starting RTI Collector Service  ^(NonSecureLAN^)
echo   Monitoring domain : 101
echo   Prometheus metrics: http://localhost:9644/metrics
echo.

set CFG_NAME=NonSecureLAN
set OBSERVABILITY_DOMAIN=101
set OBSERVABILITY_PROMETHEUS_EXPORTER_PORT=9644

call "%COLLECTOR%"
endlocal
