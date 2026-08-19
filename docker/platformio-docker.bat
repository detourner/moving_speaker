@echo off
setlocal enabledelayedexpansion

set "DEFAULT_TARGET=esp32_4m"

rem Resolve script and workspace directories.
for %%I in ("%~dp0.") do set "SCRIPT_DIR=%%~fI"
for %%I in ("%SCRIPT_DIR%\..") do set "WORKSPACE_DIR=%%~fI"

set "COMMAND=%~1"
if "%COMMAND%"=="" set "COMMAND=build"

if /I "%COMMAND%"=="help" goto do_help
if /I "%COMMAND%"=="-h" goto do_help
if /I "%COMMAND%"=="--help" goto do_help
if /I "%COMMAND%"=="list-targets" goto do_list_targets

set "TARGET=%~2"
if "%TARGET%"=="" set "TARGET=%DEFAULT_TARGET%"

call :validate_target "%TARGET%"
if errorlevel 1 goto end

call :set_image_name
if errorlevel 1 goto end

set "BASE_DOCKERFILE=%SCRIPT_DIR%\Dockerfile.base"
set "TARGET_DOCKERFILE=%SCRIPT_DIR%\Dockerfile.%TARGET%"
set "DOCKER_MOUNTS=-v "%WORKSPACE_DIR%:/workspace" -v "%PIO_CACHE_VOLUME%:/root/.platformio""
set "DOCKER_RUN_BASE=docker run --rm %DOCKER_MOUNTS% -w /workspace"

if /I "%COMMAND%"=="build" goto do_build
if /I "%COMMAND%"=="env" goto do_env
if /I "%COMMAND%"=="shell" goto do_shell
if /I "%COMMAND%"=="rebuild" goto do_rebuild
if /I "%COMMAND%"=="cache-clean" goto do_cache_clean
if /I "%COMMAND%"=="container-clean" goto do_container_clean
if /I "%COMMAND%"=="archive" goto do_archive
if /I "%COMMAND%"=="load-archive" goto do_load_archive

echo Unknown option: %COMMAND%
echo.
goto do_help

:do_build
call :ensure_target_image
if errorlevel 1 goto end
echo [build] Running PlatformIO build for target %TARGET% inside container...
%DOCKER_RUN_BASE% %IMAGE% pio run -e %TARGET%
goto end

:do_env
call :ensure_target_image
if errorlevel 1 goto end
echo [env] PlatformIO environment information for target %TARGET%...
%DOCKER_RUN_BASE% %IMAGE% pio system info
if errorlevel 1 goto end
echo.
echo ==========================
echo.
%DOCKER_RUN_BASE% %IMAGE% pio pkg list -d /workspace -e %TARGET%
goto end

:do_shell
call :ensure_target_image
if errorlevel 1 goto end
echo [shell] Opening a shell in the container for target %TARGET%...
docker run --rm -it %DOCKER_MOUNTS% -w /workspace -e MOVING_SPEAKER_TARGET=%TARGET% %IMAGE% /bin/bash
goto end

:do_rebuild
call :purge_cache
if errorlevel 1 goto end
echo [rebuild] Rebuilding base image %BASE_IMAGE%...
docker build --pull --no-cache -f "%BASE_DOCKERFILE%" -t %BASE_IMAGE% "%WORKSPACE_DIR%"
if errorlevel 1 goto end
echo [rebuild] Rebuilding target image %IMAGE%...
docker build --no-cache -f "%TARGET_DOCKERFILE%" --build-arg BASE_IMAGE=%BASE_IMAGE% -t %IMAGE% "%WORKSPACE_DIR%"
goto end

:do_cache_clean
call :purge_cache
if errorlevel 1 goto end
echo [cache-clean] PlatformIO cache cleaned for target %TARGET%.
goto end

:do_container_clean
set "REMOVED_CONTAINER=0"
echo [container-clean] Removing containers based on %IMAGE%...
for /f "delims=" %%C in ('docker ps -a --filter "ancestor=%IMAGE%" -q') do (
    set "REMOVED_CONTAINER=1"
    docker rm -f "%%C" >nul
)

if "!REMOVED_CONTAINER!"=="0" (
    echo [container-clean] No containers to remove.
) else (
    echo [container-clean] Containers removed.
)

echo [container-clean] Removing image %IMAGE%...
docker image rm -f %IMAGE% >nul 2>&1
if errorlevel 1 (
    docker image inspect %IMAGE% >nul 2>&1
    if not errorlevel 1 (
        echo Failed: could not remove image %IMAGE%.
        goto end
    )
    echo [container-clean] Image already absent.
    goto end
)

echo [container-clean] Image removed.
goto end

:do_archive
call :ensure_target_image
if errorlevel 1 goto end
set "TS=%date:~6,4%%date:~3,2%%date:~0,2%-%time:~0,2%%time:~3,2%%time:~6,2%"
set "TS=%TS: =0%"
set "ARCHIVE_BASE=%WORKSPACE_DIR%\docker\%IMAGE::=-%-%TS%"
set "ARCHIVE_IMAGE_FILE=%ARCHIVE_BASE%-image.tar"
set "ARCHIVE_CACHE_FILE=%ARCHIVE_BASE%-cache.tar"

echo [archive] Saving image to "%ARCHIVE_IMAGE_FILE%"...
docker save -o "%ARCHIVE_IMAGE_FILE%" %IMAGE%
if errorlevel 1 (
    echo [archive] Archive save failed.
    goto end
)

call :archive_cache_volume "%ARCHIVE_CACHE_FILE%"
if errorlevel 1 goto end

echo [archive] Image archive created: "%ARCHIVE_IMAGE_FILE%"
echo [archive] Cache archive created: "%ARCHIVE_CACHE_FILE%"
goto end

:do_load_archive
if "%~3"=="" (
    echo Usage: platformio-docker.bat load-archive ^<target^> ^<image.tar^> [cache.tar]
    goto end
)

set "ARCHIVE_INPUT=%~f3"
if not exist "%ARCHIVE_INPUT%" (
    echo Failed: archive not found: "%ARCHIVE_INPUT%"
    goto end
)

echo [load-archive] Loading "%ARCHIVE_INPUT%" for target %TARGET%...
docker load -i "%ARCHIVE_INPUT%"
if errorlevel 1 (
    echo Failed: could not load archive via docker load.
    echo Ensure the .tar file comes from docker save.
    goto end
)

docker image inspect %IMAGE% >nul 2>&1
if errorlevel 1 (
    echo Failed: load completed but image %IMAGE% was not found.
    echo Ensure the archive contains tag %IMAGE%.
    goto end
)

echo [load-archive] Archive loaded, build image available: %IMAGE%.

set "CACHE_ARCHIVE_INPUT=%~f4"
if "%CACHE_ARCHIVE_INPUT%"=="" (
    set "CACHE_ARCHIVE_INPUT=%ARCHIVE_INPUT:-image.tar=-cache.tar%"
)

if exist "%CACHE_ARCHIVE_INPUT%" (
    call :restore_cache_volume "%CACHE_ARCHIVE_INPUT%"
    if errorlevel 1 goto end
    echo [load-archive] Cache archive restored: "%CACHE_ARCHIVE_INPUT%"
) else (
    echo [load-archive] Cache archive not found, skipped: "%CACHE_ARCHIVE_INPUT%"
)

goto end

:do_help
echo Usage: platformio-docker.bat [command] [target] [archive args]
echo.
echo Commands:
echo   build [target]           Build the requested PlatformIO target. Default target: %DEFAULT_TARGET%
echo   env [target]             Run pio system info and pio pkg list for the target
echo   shell [target]           Open a shell in the target container
echo   rebuild [target]         Clean target cache and rebuild base + target images
echo   cache-clean [target]     Clean the Docker volume used for the target PlatformIO cache
echo   container-clean [target] Remove target containers and the target image
echo   archive [target]         Save target Docker image and PlatformIO cache volume to .tar archives
echo   load-archive [target] ^<image.tar^> [cache.tar]
echo                           Load target image archive and optional cache archive
echo   list-targets             Show supported targets
echo   help                     Show this help
echo.
echo Targets:
echo   esp32_4m
echo   avr_2m
goto end

:do_list_targets
echo Supported targets:
echo   esp32_4m
echo   avr_2m
goto end

:archive_cache_volume
set "CACHE_ARCHIVE_TARGET=%~f1"

docker volume inspect "%PIO_CACHE_VOLUME%" >nul 2>&1
if errorlevel 1 (
    echo [archive] Cache volume not found, creating empty volume: "%PIO_CACHE_VOLUME%"
    docker volume create "%PIO_CACHE_VOLUME%" >nul
    if errorlevel 1 (
        echo Failed: could not create cache volume "%PIO_CACHE_VOLUME%".
        exit /b 1
    )
)

echo [archive] Saving cache volume to "%CACHE_ARCHIVE_TARGET%"...
docker run --rm -v "%PIO_CACHE_VOLUME%:/cache" alpine:3.20 sh -c "cd /cache && tar -cf - ." > "%CACHE_ARCHIVE_TARGET%"
if errorlevel 1 (
    echo Failed: could not archive cache volume "%PIO_CACHE_VOLUME%".
    exit /b 1
)

exit /b 0

:restore_cache_volume
set "CACHE_ARCHIVE_SOURCE=%~f1"

if not exist "%CACHE_ARCHIVE_SOURCE%" (
    echo Failed: cache archive not found: "%CACHE_ARCHIVE_SOURCE%"
    exit /b 1
)

docker volume create "%PIO_CACHE_VOLUME%" >nul
if errorlevel 1 (
    echo Failed: could not create cache volume "%PIO_CACHE_VOLUME%".
    exit /b 1
)

echo [load-archive] Restoring cache volume from "%CACHE_ARCHIVE_SOURCE%"...
docker run --rm -i -v "%PIO_CACHE_VOLUME%:/cache" alpine:3.20 sh -c "rm -rf /cache/* /cache/.[!.]* /cache/..?* 2>/dev/null; cd /cache && tar -xf -" < "%CACHE_ARCHIVE_SOURCE%"
if errorlevel 1 (
    echo Failed: could not restore cache volume "%PIO_CACHE_VOLUME%".
    exit /b 1
)

exit /b 0

:validate_target
set "TARGET=%~1"
if /I "%TARGET%"=="esp32_4m" exit /b 0
if /I "%TARGET%"=="avr_2m" exit /b 0
echo Failed: unsupported target "%TARGET%".
echo Supported targets: esp32_4m, avr_2m
exit /b 1

:set_image_name
set "REPO_ID="
set "REPO_NAME="
set "IMAGE_BASE="

for /f "delims=" %%U in ('git -C "%WORKSPACE_DIR%" config --get remote.origin.url 2^>nul') do (
    set "REPO_ID=%%U"
)

if not "!REPO_ID!"=="" (
    set "REPO_ID=!REPO_ID:.git=!"
    set "REPO_ID=!REPO_ID:/=-!"
    set "REPO_ID=!REPO_ID::=-!"
    set "REPO_ID=!REPO_ID:@=-!"
    set "REPO_ID=!REPO_ID: =-!"
    set "REPO_ID=!REPO_ID:_=-!"
    set "REPO_NAME=!REPO_ID!"
)

if "%REPO_NAME%"=="" (
    for /f "delims=" %%R in ('git -C "%WORKSPACE_DIR%" rev-parse --show-toplevel 2^>nul') do (
        for %%N in ("%%R") do set "REPO_NAME=%%~nxN"
    )
)

if "%REPO_NAME%"=="" (
    for %%N in ("%WORKSPACE_DIR%") do set "REPO_NAME=%%~nxN"
)

if "%REPO_NAME%"=="" (
    echo Failed: could not determine repository/workspace name.
    exit /b 1
)

set "IMAGE_BASE=%REPO_NAME%"
set "IMAGE_BASE=%IMAGE_BASE: =-%"
set "IMAGE_BASE=%IMAGE_BASE:_=-%"
set "IMAGE_BASE=%IMAGE_BASE:.=-%"
set "IMAGE_BASE=%IMAGE_BASE:/=-%"
set "IMAGE_BASE=%IMAGE_BASE:\=-%"
set "IMAGE_BASE=%IMAGE_BASE::=-%"
set "IMAGE_BASE=%IMAGE_BASE:@=-%"

set "IMAGE_BASE=!IMAGE_BASE:A=a!"
set "IMAGE_BASE=!IMAGE_BASE:B=b!"
set "IMAGE_BASE=!IMAGE_BASE:C=c!"
set "IMAGE_BASE=!IMAGE_BASE:D=d!"
set "IMAGE_BASE=!IMAGE_BASE:E=e!"
set "IMAGE_BASE=!IMAGE_BASE:F=f!"
set "IMAGE_BASE=!IMAGE_BASE:G=g!"
set "IMAGE_BASE=!IMAGE_BASE:H=h!"
set "IMAGE_BASE=!IMAGE_BASE:I=i!"
set "IMAGE_BASE=!IMAGE_BASE:J=j!"
set "IMAGE_BASE=!IMAGE_BASE:K=k!"
set "IMAGE_BASE=!IMAGE_BASE:L=l!"
set "IMAGE_BASE=!IMAGE_BASE:M=m!"
set "IMAGE_BASE=!IMAGE_BASE:N=n!"
set "IMAGE_BASE=!IMAGE_BASE:O=o!"
set "IMAGE_BASE=!IMAGE_BASE:P=p!"
set "IMAGE_BASE=!IMAGE_BASE:Q=q!"
set "IMAGE_BASE=!IMAGE_BASE:R=r!"
set "IMAGE_BASE=!IMAGE_BASE:S=s!"
set "IMAGE_BASE=!IMAGE_BASE:T=t!"
set "IMAGE_BASE=!IMAGE_BASE:U=u!"
set "IMAGE_BASE=!IMAGE_BASE:V=v!"
set "IMAGE_BASE=!IMAGE_BASE:W=w!"
set "IMAGE_BASE=!IMAGE_BASE:X=x!"
set "IMAGE_BASE=!IMAGE_BASE:Y=y!"
set "IMAGE_BASE=!IMAGE_BASE:Z=z!"

set "BASE_IMAGE=platformio-!IMAGE_BASE!-base:1.0"
set "IMAGE=platformio-!IMAGE_BASE!-%TARGET%:1.0"
set "PIO_CACHE_VOLUME=pio-cache-!IMAGE_BASE!-%TARGET%"
echo [init] Base Docker image: %BASE_IMAGE%
echo [init] Target Docker image: %IMAGE%
echo [init] PlatformIO cache volume: %PIO_CACHE_VOLUME%
exit /b 0

:ensure_target_image
call :ensure_base_image
if errorlevel 1 exit /b 1

if not exist "%TARGET_DOCKERFILE%" (
    echo Failed: target Dockerfile not found: "%TARGET_DOCKERFILE%"
    exit /b 1
)

docker image inspect %IMAGE% >nul 2>&1
if not errorlevel 1 exit /b 0

echo [build] Target image %IMAGE% not found, creating it...
docker build -f "%TARGET_DOCKERFILE%" --build-arg BASE_IMAGE=%BASE_IMAGE% -t %IMAGE% "%WORKSPACE_DIR%"
if errorlevel 1 (
    echo Failed: could not create image %IMAGE%.
    exit /b 1
)

echo [build] Target image %IMAGE% created.
exit /b 0

:ensure_base_image
if not exist "%BASE_DOCKERFILE%" (
    echo Failed: base Dockerfile not found: "%BASE_DOCKERFILE%"
    exit /b 1
)

docker image inspect %BASE_IMAGE% >nul 2>&1
if not errorlevel 1 exit /b 0

echo [build] Base image %BASE_IMAGE% not found, creating it...
docker build -f "%BASE_DOCKERFILE%" -t %BASE_IMAGE% "%WORKSPACE_DIR%"
if errorlevel 1 (
    echo Failed: could not create base image %BASE_IMAGE%.
    exit /b 1
)

echo [build] Base image %BASE_IMAGE% created.
exit /b 0

:purge_cache
echo [cache] Cleaning PlatformIO Docker volume: "%PIO_CACHE_VOLUME%"
docker volume rm -f "%PIO_CACHE_VOLUME%" >nul
if errorlevel 1 (
    echo Failed: could not remove volume "%PIO_CACHE_VOLUME%".
    exit /b 1
)
exit /b 0

:end
endlocal