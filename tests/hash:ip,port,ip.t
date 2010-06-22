# Create a set with timeout
0 ipset create test hash:ip,port,ip timeout 5
# Add partly zero valued element
0 ipset add test 2.0.0.1,0,0.0.0.0
# Test partly zero valued element
0 ipset test test 2.0.0.1,0,0.0.0.0
# Delete party zero valued element
0 ipset del test 2.0.0.1,0,0.0.0.0
# Add almost zero valued element
0 ipset add test 2.0.0.1,0,0.0.0.1
# Test almost zero valued element
0 ipset test test 2.0.0.1,0,0.0.0.1
# Delete almost zero valued element
0 ipset del test 2.0.0.1,0,0.0.0.1
# Add lower boundary
0 ipset add test 2.0.0.1,5,1.1.1.1
# Add upper boundary
0 ipset add test 2.1.0.0,128,2.2.2.2
# Test lower boundary
0 ipset test test 2.0.0.1,5,1.1.1.1
# Test upper boundary
0 ipset test test 2.1.0.0,128,2.2.2.2
# Test value not added to the set
1 ipset test test 2.0.0.1,5,1.1.1.2
# Test value not added to the set
1 ipset test test 2.0.0.1,6,1.1.1.1
# Test value not added to the set
1 ipset test test 2.0.0.2,6,1.1.1.1
# Test value before lower boundary
1 ipset test test 2.0.0.0,5,1.1.1.1
# Test value after upper boundary
1 ipset test test 2.1.0.1,128,2.2.2.2
# Try to add value before lower boundary
0 ipset add test 2.0.0.0,5,1.1.1.1
# Try to add value after upper boundary
0 ipset add test 2.1.0.1,128,2.2.2.2
# List set
0 ipset list test | sed 's/timeout ./timeout x/' > .foo0 && ./sort.sh .foo0
# Check listing
0 diff -I 'Size in memory.*' .foo hash:ip,port,ip.t.list0 && rm .foo
# Sleep 6s so that elements can time out
0 sleep 6
# List set
0 ipset list test > .foo0 && ./sort.sh .foo0
# Check listing
0 diff -I 'Size in memory.*' .foo hash:ip,port,ip.t.list1 && rm .foo
# Flush test set
0 ipset flush test
# Delete test set
0 ipset destroy test
# eof
