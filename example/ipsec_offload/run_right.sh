#!/bin/bash
#

./odp_ipsec_offload -f 64 -i dpni.1,dpni.2 \
-r 192.168.111.2/32:dpni.1:00.00.00.00.00.2 \
-r 192.168.222.2/32:dpni.2:00.10.94.00.00.03 \
-p 192.168.111.2:192.168.222.2:in:both \
-e 192.168.111.2:192.168.222.2:aes:201:656c8523255ccc23a66c1917aa0cf309 \
-a 192.168.111.2:192.168.222.2:sha256:201:a731649644c5dee92cbd9c2e7e188ee6aa0cf309a731649644c5dee92cbd9c2e \
-t 192.168.111.2:192.168.222.2:192.168.100.1:192.168.200.1 \
-p 192.168.222.2:192.168.111.2:out:both \
-e 192.168.222.2:192.168.111.2:aes:201:656c8523255ccc23a66c1917aa0cf309 \
-a 192.168.222.2:192.168.111.2:sha256:201:a731649644c5dee92cbd9c2e7e188ee6aa0cf309a731649644c5dee92cbd9c2e \
-t 192.168.222.2:192.168.111.2:192.168.200.1:192.168.100.1 &
