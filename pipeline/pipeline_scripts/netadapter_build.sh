#!/bin/bash

# define paths
BRANCH=$GRAPH_SOLVERS_BRANCH
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PWD_STORE="${SCRIPT_DIR}/../.."

# Проверяем, передан ли аргумент (GCC, MinGW или Clang)
if [ -z "$1" ]; then
    echo "Usage: $0 [GCC|MinGW|Clang]"
    exit 1
fi

COMPILER=$1

case $COMPILER in
    GCC)
        LOG_DIR="${SCRIPT_DIR}/../pipeline_result/"
        NETADAPTER_LOG_NAME="netadapter_build_gcc.log"
        ;;
    MinGW)
        LOG_DIR="${SCRIPT_DIR}/../pipeline_result/"
        NETADAPTER_LOG_NAME="netadapter_build_mingw.log"
        ;;
    Clang)
        LOG_DIR="${SCRIPT_DIR}/../pipeline_result/"
        NETADAPTER_LOG_NAME="netadapter_build_clang.log"
        ;;
    *)
        echo "Invalid compiler! Use GCC, MinGW, or Clang."
        exit 1
        ;;
esac

mkdir -p "${LOG_DIR}"
NETADAPTER_LOG="${LOG_DIR}/${NETADAPTER_LOG_NAME}"

echo "Building .NET adapter components for $COMPILER..."

# Сборка GraphSolversNet
echo "Building GraphSolversNet..."
cd "${PWD_STORE}/netadapter/GraphSolversNet"
dotnet build --configuration Release >> "${NETADAPTER_LOG}" 2>&1
NET_BUILD_RESULT=$?

if [ $NET_BUILD_RESULT -ne 0 ]; then
    echo "--------------- Result ---------------"
	echo "Ошибка: сборка GraphSolversNet [$BRANCH] не удалась"
    echo "--------------------------------------"
	exit 1
else
    echo "GraphSolversNet успешно собран"
fi

# Сборка Tests
echo "Building Tests..."
cd "${PWD_STORE}/netadapter/Tests"
dotnet build --configuration Release >> "${NETADAPTER_LOG}" 2>&1
TESTS_BUILD_RESULT=$?

if [ $TESTS_BUILD_RESULT -ne 0 ]; then
    echo "--------------- Result ---------------"
	echo "Ошибка: сборка Tests [$BRANCH] не удалась"
    echo "--------------------------------------"
	exit 1
else
    echo "Tests успешно собран"
fi
