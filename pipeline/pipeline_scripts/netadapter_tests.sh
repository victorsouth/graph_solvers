#!/bin/sh

# Получаем абсолютный путь к директории, в которой находится этот скрипт
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
RES_DIR="${SCRIPT_DIR}/../pipeline_result/dotnet_tests_out"
mkdir -p ${RES_DIR}
cd "${SCRIPT_DIR}/../../netadapter/Tests"
TEST_NAME="netadapter_tests"

dotnet test --configuration Release \
    --logger "trx;LogFileName=${RES_DIR}/${TEST_NAME}_out.xml" \
    --logger "console;verbosity=normal" \
    > ${RES_DIR}/${TEST_NAME}_out.txt 2>&1
TEST_RETURN=$?

# Проверка на случай отсутствия test case
NO_TESTS_MSG=$(grep -E 'No test is available|No tests found|This test program does NOT link in any test case' "${RES_DIR}/${TEST_NAME}_out.txt")

if [ -n "$NO_TESTS_MSG" ]; then
    echo "---------- Test program has no test cases ----------"
    echo "$NO_TESTS_MSG"
    echo "This is considered successful execution."
    echo "-------------------------------------------"
    exit 0
fi

# Проверка на наличие сообщений об аварийном завершении
CRASH_ERRORS=$(grep -E 'terminate called after throwing|std::logic_error|Segmentation fault|Aborted|Unhandled exception' ${RES_DIR}/${TEST_NAME}_out.txt || true)
# Признак завершения тестового запуска (англ. и рус. локали)
TEST_RUN_COMPLETE=$(grep -qE 'Test Run|Тестовый запуск|Всего тестов|Пройдено|Провалено' ${RES_DIR}/${TEST_NAME}_out.txt && echo "yes" || echo "no")
# Неудачные тесты: по итоговой строке (Failed: 1+ или Провалено: 1+), без ложных срабатываний на "exception"
failed_tests=$(grep -E 'Failed: [1-9]|Провалено: [1-9]' ${RES_DIR}/${TEST_NAME}_out.txt || true)

if [[ -n "$failed_tests" || -n "$CRASH_ERRORS" || $TEST_RETURN -ne 0 || "$TEST_RUN_COMPLETE" = "no" ]]; then
    echo "---------- Failed tests or crashes found ----------"
    if [[ "$TEST_RUN_COMPLETE" = "yes" ]]; then
		tac ${RES_DIR}/${TEST_NAME}_out.txt | awk '/Test Run/{exit} 1' | tac
	fi
    if [[ -n "$CRASH_ERRORS" ]]; then
        echo "Test failure detected:"
        echo "$CRASH_ERRORS"
		echo "More: ${TEST_NAME}_out.txt"
    fi
	echo "------------------------------------------------------"
    echo "--------------- List of tests ---------------"
    dotnet test --list-tests --configuration Release 2>/dev/null || echo "Could not list tests"
    echo "-------------------------------------------"
    exit 1
else
    echo "---------- All tests were successful ---------"
    tail -30 ${RES_DIR}/${TEST_NAME}_out.txt | grep -E "Test Run|Passed|Failed|Total|Summary" || tail -20 ${RES_DIR}/${TEST_NAME}_out.txt
    echo "-------------------------------------------"
    echo "--------------- List of tests ---------------"
    dotnet test --list-tests --configuration Release 2>/dev/null || echo "Could not list tests"
    echo "-------------------------------------------"
fi

