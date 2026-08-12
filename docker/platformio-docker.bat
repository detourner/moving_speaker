@echo off
setlocal enabledelayedexpansion

rem Resolve script and workspace directories.
for %%I in ("%~dp0.") do set "SCRIPT_DIR=%%~fI"
for %%I in ("%SCRIPT_DIR%\..") do set "WORKSPACE_DIR=%%~fI"

rem Build a repo-specific image/cache identity.
call :set_image_name
if errorlevel 1 goto end
set "DOCKERFILE_DIR=%SCRIPT_DIR%"

rem Shared docker run arguments for all commands.
set "DOCKER_MOUNTS=-v "%WORKSPACE_DIR%:/workspace" -v "%PIO_CACHE_VOLUME%:/root/.platformio""
set "DOCKER_RUN_BASE=docker run --rm %DOCKER_MOUNTS% -w /workspace %IMAGE%"

if "%~1"=="" goto do_build

rem Command dispatcher.
if /I "%~1"=="build" goto do_build
if /I "%~1"=="env" goto do_env
if /I "%~1"=="shell" goto do_shell
if /I "%~1"=="rebuild" goto do_rebuild
if /I "%~1"=="cache-clean" goto do_cache_clean
if /I "%~1"=="container-clean" goto do_container_clean
if /I "%~1"=="archive" goto do_archive
if /I "%~1"=="load-archive" goto do_load_archive
if /I "%~1"=="help" goto do_help
if /I "%~1"=="-h" goto do_help
if /I "%~1"=="--help" goto do_help

echo Unknown option: %~1
echo.
goto do_help

:do_build
rem Ensure image exists before launching PlatformIO build.
call :ensure_image
if errorlevel 1 goto end
echo [build] Running PlatformIO build inside container...
%DOCKER_RUN_BASE% pio run
goto end

:do_env
echo [env] PlatformIO environment information...
%DOCKER_RUN_BASE% pio system info
if errorlevel 1 goto end
echo.
echo ==========================
echo.
%DOCKER_RUN_BASE% pio pkg list
goto end

:do_shell
echo [shell] Opening a shell in the container...
docker run --rm -it %DOCKER_MOUNTS% -w /workspace %IMAGE% /bin/bash
goto end

:do_rebuild
rem Full rebuild path: clear cache then force image rebuild.
call :purge_cache
if errorlevel 1 goto end
echo [rebuild] Forcing rebuild of image %IMAGE%...
docker build --pull --no-cache -t %IMAGE% "%DOCKERFILE_DIR%"
goto end

:do_cache_clean
call :purge_cache
if errorlevel 1 goto end
echo [cache-clean] PlatformIO cache cleaned.
goto end

:do_container_clean
set "REMOVED_CONTAINER=0"
rem Remove stopped/running containers created from the image.
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
rem Create timestamped archives for both image and cache volume.
set "TS=%date:~6,4%%date:~3,2%%date:~0,2%-%time:~0,2%%time:~3,2%%time:~6,2%"
set "TS=%TS: =0%"
set "ARCHIVE_BASE=%WORKSPACE_DIR%\docker\%IMAGE::=-%-%TS%"
set "ARCHIVE_IMAGE_FILE=%ARCHIVE_BASE%-image.tar"
set "ARCHIVE_CACHE_FILE=%ARCHIVE_BASE%-cache.tar"

echo [archive] Checking image %IMAGE%...
docker image inspect %IMAGE% >nul 2>&1
if errorlevel 1 (
    echo Failed: image not found: %IMAGE%.
    goto end
)

echo [archive] Saving image to "%ARCHIVE_IMAGE_FILE%"...
docker save -o "%ARCHIVE_IMAGE_FILE%" %IMAGE%
set "ARCHIVE_RC=%ERRORLEVEL%"

if not "%ARCHIVE_RC%"=="0" (
    echo [archive] Archive save failed.
    goto end
)

call :archive_cache_volume "%ARCHIVE_CACHE_FILE%"
if errorlevel 1 goto end

echo [archive] Image archive created: "%ARCHIVE_IMAGE_FILE%"
echo [archive] Cache archive created: "%ARCHIVE_CACHE_FILE%"
goto end

:do_load_archive
rem Load image archive and optionally restore cache volume archive.
if "%~2"=="" (
    echo Usage: build.bat load-archive ^<image.tar^> [cache.tar]
    goto end
)

set "ARCHIVE_INPUT=%~f2"
if not exist "%ARCHIVE_INPUT%" (
    echo Failed: archive not found: "%ARCHIVE_INPUT%"
    goto end
)

echo [load-archive] Loading "%ARCHIVE_INPUT%"...
docker load -i "%ARCHIVE_INPUT%"
if errorlevel 1 (
    echo [load-archive] Format is not compatible with docker load. Trying docker import...
    docker import "%ARCHIVE_INPUT%" %IMAGE% >nul
    if errorlevel 1 (
        echo Failed: could not load archive via docker load/import.
        echo Ensure the .tar file comes from docker save or docker export.
        goto end
    )
)

docker image inspect %IMAGE% >nul 2>&1
if errorlevel 1 (
    echo Failed: load completed but image %IMAGE% was not found.
    echo Ensure the archive contains tag %IMAGE%.
    goto end
)

echo [load-archive] Archive loaded, build image available: %IMAGE%.

set "CACHE_ARCHIVE_INPUT=%~f3"
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
echo Usage: build.bat [option]
echo.
echo no option        : build the PlatformIO project
echo build            : build the PlatformIO project
echo env              : run pio system info + pio pkg list (no build)
echo shell            : open a shell in the container
echo rebuild          : clean PlatformIO cache + rebuild Docker image
echo cache-clean      : clean the Docker volume used for PlatformIO cache
echo container-clean  : remove build-image containers and the build image
echo archive          : save Docker image + PlatformIO cache volume to .tar archives
echo load-archive     : load image archive and optional cache archive
echo help             : show this help

goto end

:archive_cache_volume
set "CACHE_ARCHIVE_TARGET=%~f1"

rem Ensure cache volume exists so archive command is deterministic.
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

:set_image_name
rem Derive a unique identity from git remote when available.
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
    rem Fallback to repository root folder name.
    for /f "delims=" %%R in ('git -C "%WORKSPACE_DIR%" rev-parse --show-toplevel 2^>nul') do (
        for %%N in ("%%R") do set "REPO_NAME=%%~nxN"
    )
)

if "%REPO_NAME%"=="" (
    rem Last fallback to workspace folder name.
    for %%N in ("%WORKSPACE_DIR%") do set "REPO_NAME=%%~nxN"
)

if "%REPO_NAME%"=="" (
    echo Failed: could not determine repository/workspace name.
    exit /b 1
)

set "IMAGE_BASE=%REPO_NAME%"

rem Docker image names must be lowercase and use [a-z0-9._-]
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

set "IMAGE=platformio-!IMAGE_BASE!:1.0"
set "PIO_CACHE_VOLUME=pio-cache-!IMAGE_BASE!"
echo [init] Docker image: %IMAGE%
echo [init] PlatformIO cache volume: %PIO_CACHE_VOLUME%
exit /b 0

:ensure_image
rem Auto-create the build image when it does not exist.
docker image inspect %IMAGE% >nul 2>&1
if not errorlevel 1 (
    exit /b 0
)

echo [build] Image %IMAGE% not found, creating it...
docker build -t %IMAGE% "%DOCKERFILE_DIR%"
if errorlevel 1 (
    echo Failed: could not create image %IMAGE%.
    exit /b 1
)

echo [build] Image %IMAGE% created.
exit /b 0

:purge_cache
rem Delete the repo-specific PlatformIO cache volume.
echo [cache] Cleaning PlatformIO Docker volume: "%PIO_CACHE_VOLUME%"
docker volume rm -f "%PIO_CACHE_VOLUME%" >nul
if errorlevel 1 (
    echo Failed: could not remove volume "%PIO_CACHE_VOLUME%".
    exit /b 1
)
exit /b 0

:end
endlocal