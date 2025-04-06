cd ..
## make sure to run this script in the root directory of the project
# 统计 .cc 文件行数
find ./ -name "*.cc" -exec wc -l {} + | awk '{sum += $1} END {print "CC:", sum}'

# 统计 .h 文件行数
find ./ -name "*.h" -exec wc -l {} + | awk '{sum += $1} END {print "H:", sum}'
