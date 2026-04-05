#!/bin/bash
# Pattern testing script for Kaypro CRT

echo "=== Testing Convergence Patterns ==="
echo ""

echo "Pattern 0: Horizontal Stripes"
echo "0" > /dev/ttyACM0
sleep 3

echo "Pattern 1: Crosshair (center alignment)"
echo "1" > /dev/ttyACM0
sleep 3

echo "Pattern 2: Grid (geometry test)"
echo "2" > /dev/ttyACM0
sleep 3

echo "Pattern 3: Circles (focus test)"
echo "3" > /dev/ttyACM0
sleep 3

echo "Pattern 4: Corner Markers (overscan)"
echo "4" > /dev/ttyACM0
sleep 3

echo "Pattern 5: Border (edge test)"
echo "5" > /dev/ttyACM0
sleep 3

echo "Pattern 6: Checkerboard"
echo "6" > /dev/ttyACM0
sleep 3

echo "Pattern 7: Full White"
echo "7" > /dev/ttyACM0
sleep 3

echo "Pattern 8: Full Black"
echo "8" > /dev/ttyACM0
sleep 3

echo ""
echo "Test complete! Returning to crosshair..."
echo "1" > /dev/ttyACM0
