#!/bin/bash

# Configuration
SERVER_URL="http://localhost:8080"
DB_FILE="compiled/SDM.db"
INDEX_DIR="compiled/indexes"

# Colors
GREEN='\033[0;32m'
RED='\033[0;31m'
NC='\033[0m'

echo "=== Persistence Verification Script ==="

# 1. Clean verify
echo -e "\n1. Stopping server and cleaning previous data..."
pkill sdm_server
sleep 2

rm -f $DB_FILE
rm -rf $INDEX_DIR
mkdir -p $INDEX_DIR

echo "Starting server..."
./sdm_server --server > server_log.txt 2>&1 &
SERVER_PID=$!
echo "Server started with PID $SERVER_PID"
# Wait for server to initialize
sleep 5

# Function to make POST request
make_request() {
    curl -s -X POST $SERVER_URL -H "Content-Type: application/json" -d "$1"
}

# 2. Register User
echo -e "\n2. Registering User..."
REGISTER_RES=$(make_request '{"operation":"user_register","username":"persist_test","password":"Password123!","full_name":"Persistence Tester","license_number":"P123456"}')
echo "Response: $REGISTER_RES"

# 3. Login
echo -e "\n3. Logging in..."
LOGIN_RES=$(make_request '{"operation":"user_login","username":"persist_test","password":"Password123!"}')
SESSION_ID=$(echo $LOGIN_RES | grep -o '"session_id":"[^"]*"' | cut -d'"' -f4)
DRIVER_ID=$(echo $LOGIN_RES | grep -o '"driver_id":"[^"]*"' | cut -d'"' -f4)
echo "Session ID: $SESSION_ID"
echo "Driver ID: $DRIVER_ID"

if [ -z "$SESSION_ID" ]; then
    echo -e "${RED}Login failed!${NC}"
    exit 1
fi

# 4. Create Data (Vehicle, Expense, Incident)
echo -e "\n4. Creating Data..."

# Vehicle
echo "Adding Vehicle..."
VEHICLE_RES=$(make_request "{\"operation\":\"vehicle_add\",\"session_id\":\"$SESSION_ID\",\"make\":\"Toyota\",\"model\":\"Camry\",\"year\":\"2022\",\"license_plate\":\"PER-001\",\"type\":\"0\"}")
VEHICLE_ID=$(echo $VEHICLE_RES | grep -o '"vehicle_id":[^,}]*' | cut -d':' -f2 | tr -d ' "')
echo "Vehicle ID: $VEHICLE_ID"

# Expense
echo "Adding Expense..."
EXPENSE_RES=$(make_request "{\"operation\":\"expense_add\",\"session_id\":\"$SESSION_ID\",\"vehicle_id\":\"$VEHICLE_ID\",\"category\":\"0\",\"amount\":\"50.00\",\"description\":\"Test Fuel\"}")
echo "Expense Response: $EXPENSE_RES"

# Incident
echo "Reports Incident..."
INCIDENT_RES=$(make_request "{\"operation\":\"incident_report\",\"session_id\":\"$SESSION_ID\",\"vehicle_id\":\"$VEHICLE_ID\",\"type\":\"0\",\"description\":\"Test Scratch\",\"latitude\":31.5,\"longitude\":74.3}")
echo "Incident Response: $INCIDENT_RES"

# Trip
echo "Starting Trip..."
TRIP_START_RES=$(make_request "{\"operation\":\"trip_start\",\"session_id\":\"$SESSION_ID\",\"driver_id\":$DRIVER_ID,\"vehicle_id\":$VEHICLE_ID,\"latitude\":31.5,\"longitude\":74.3,\"address\":\"Start Point\"}")
TRIP_ID=$(echo $TRIP_START_RES | grep -o '"trip_id":[^,}]*' | cut -d':' -f2 | tr -d ' "')
echo "Trip ID: $TRIP_ID"

# 5. Verify Data Exists
echo -e "\n5. Verifying Data Exists (Before Restart)..."
SUMMARY_RES=$(make_request "{\"operation\":\"expense_get_summary\",\"session_id\":\"$SESSION_ID\"}")
echo "Expense Summary: $SUMMARY_RES"

ACTIVE_TRIP_RES=$(make_request "{\"operation\":\"trip_get_active\",\"session_id\":\"$SESSION_ID\"}")
echo "Active Trip: $ACTIVE_TRIP_RES"


# 6. Simulate Server Restart (Stop and Start)
echo -e "\n6. Simulating Server Restart..."
kill $SERVER_PID
wait $SERVER_PID
echo "Server stopped."
sleep 2

# Start Server in background
./sdm_server --server > server_log_restart.txt 2>&1 &
SERVER_PID=$!
echo "Server started with PID $SERVER_PID"
sleep 10

# 7. Relogin (Session might be invalid if in-memory only, but testing DB persistence)
# Assuming sessions are in memory, we need to login again.
echo -e "\n7. Re-logging in after restart..."
LOGIN_RES_2=$(make_request '{"operation":"user_login","username":"persist_test","password":"Password123!"}')
SESSION_ID_2=$(echo $LOGIN_RES_2 | grep -o '"session_id":"[^"]*"' | cut -d'"' -f4)
echo "New Session ID: $SESSION_ID_2"

if [ -z "$SESSION_ID_2" ]; then
     echo -e "${RED}Re-login failed! Persistence broken?${NC}"
     # Kill server before exit
     kill $SERVER_PID
     exit 1
fi

# 8. Verify Data Persisted
echo -e "\n8. Verifying Data Persistence..."

# Expenses
SUMMARY_RES_2=$(make_request "{\"operation\":\"expense_get_summary\",\"session_id\":\"$SESSION_ID_2\"}")
echo "Expense Summary (Post-Restart): $SUMMARY_RES_2"

if [[ $SUMMARY_RES_2 == *"total_expenses"* ]]; then
    echo -e "${GREEN}Expenses Persisted!${NC}"
else
    echo -e "${RED}Expenses Missing!${NC}"
fi

# Active Trip (Should still be active if not ended)
ACTIVE_TRIP_RES_2=$(make_request "{\"operation\":\"trip_get_active\",\"session_id\":\"$SESSION_ID_2\"}")
echo "Active Trip (Post-Restart): $ACTIVE_TRIP_RES_2"

if [[ $ACTIVE_TRIP_RES_2 == *"$TRIP_ID"* ]]; then
     echo -e "${GREEN}Trip Persisted!${NC}"
else
     echo -e "${RED}Trip Missing!${NC}"
fi

# 9. Cleanup
echo -e "\n9. Cleanup..."
# End Trip
make_request "{\"operation\":\"trip_end\",\"session_id\":\"$SESSION_ID_2\",\"trip_id\":$TRIP_ID,\"latitude\":31.6,\"longitude\":74.4,\"distance\":10.0,\"address\":\"End Point\",\"duration\":3600}"

kill $SERVER_PID
echo "Server stopped."
