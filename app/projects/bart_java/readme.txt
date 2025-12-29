#
# BART Java Bindings
#
# Authors: Thies Pfeiffer, Claas Braun
#

Description:

This project holds the Java bindings for BART. 


Known Problems:
- In cmake up to 3.4 there is a problem with the expansion of the CLASSPATH for javah. The ";" will be removed and thus no more than one path can be specified. A fix is to change the classpath_sep in UseJava to \; instead of ;. A report has been filed to the cmake guys.

