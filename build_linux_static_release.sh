cd ..
rm -rf nexus-tools_build
mkdir nexus-tools_build
cd nexus-tools_build
export CXX=/usr/bin/g++-5
cmake -DLINUX_STATIC:BOOL=ON -DCMAKE_BUILD_TYPE=Release ../nexus-tools
make flexcat
make nexus-pre
make MappingAnalyzer
make 5PrimeEndCounter
cp -f bin/flexcat ../nexus-tools/bin/
cp -f bin/nexus-pre ../nexus-tools/bin/
cp -f bin/MappingAnalyzer ../nexus-tools/bin/
cp -f bin/5PrimeEndCounter ../nexus-tools/bin/



