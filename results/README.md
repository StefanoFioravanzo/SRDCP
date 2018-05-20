## Simulations config

- **01\_nullrdc**:

	- `my_collect.h`:

		TOPOLOGY\_REPORT 1 
		PIGGYBACKING 1  
		BEACON\_INTERVAL (`CLOCK_SECOND*30`)  
		BEACON\_FORWARD\_DELAY (`random_rand() % (CLOCK_SECOND*4)`)  
		TOPOLOGY\_REPORT\_HOLD\_TIME (`CLOCK_SECOND*15`)  

	- `project-conf.h`:

		NETSTACK\_RDC nullrdc\_driver  
		NETSTACK\_CONF\_RDC\_CHANNEL\_CHECK\_RATE Not Used   

- **02-contikimac-full**:

	- `my_collect.h`:

		TOPOLOGY\_REPORT 1  
		PIGGYBACKING 1  
		BEACON\_INTERVAL (`CLOCK_SECOND*30`)  
		BEACON\_FORWARD\_DELAY (`random_rand() % (CLOCK_SECOND*4)`)  
		TOPOLOGY\_REPORT\_HOLD\_TIME (`CLOCK_SECOND*15`)  

	- `project-conf.h`:

		NETSTACK\_RDC contikimac\_driver  
		NETSTACK\_CONF\_RDC\_CHANNEL\_CHECK\_RATE 8  

- **03-contikimac-cr-32**:

	- `my_collect.h`:

		TOPOLOGY\_REPORT 1  
		PIGGYBACKING 1  
		BEACON\_INTERVAL (`CLOCK_SECOND*30`)  
		BEACON\_FORWARD\_DELAY (`random_rand() % (CLOCK_SECOND*4)`)  
		TOPOLOGY\_REPORT\_HOLD\_TIME (`CLOCK_SECOND*15`)  

	- `project-conf.h`:

		NETSTACK\_RDC contikimac\_driver  
		NETSTACK\_CONF\_RDC\_CHANNEL\_CHECK\_RATE 32  

- **04-contikimac-piggy**:

	- `my_collect.h`:

		TOPOLOGY\_REPORT 0  
		PIGGYBACKING 1  
		BEACON\_INTERVAL (`CLOCK_SECOND*30`)  
		BEACON\_FORWARD\_DELAY (`random_rand() % (CLOCK_SECOND*4)`)  
		TOPOLOGY\_REPORT\_HOLD\_TIME (`CLOCK_SECOND*15`)  

	- `project-conf.h`:

		NETSTACK\_RDC contikimac\_driver  
		NETSTACK\_CONF\_RDC\_CHANNEL\_CHECK\_RATE 8  

- **05-contikimac-piggy-cr-32**:

	- `my_collect.h`:

		TOPOLOGY\_REPORT 0  
		PIGGYBACKING 1  
		BEACON\_INTERVAL (`CLOCK_SECOND*30`)  
		BEACON\_FORWARD\_DELAY (`random_rand() % (CLOCK_SECOND*4)`)  
		TOPOLOGY\_REPORT\_HOLD\_TIME (`CLOCK_SECOND*15`)  

	- `project-conf.h`:

		NETSTACK\_RDC contikimac\_driver  
		NETSTACK\_CONF\_RDC\_CHANNEL\_CHECK\_RATE 32
		
- **06-contikimac-tr**:

	- `my_collect.h`:

		TOPOLOGY\_REPORT 1  
		PIGGYBACKING 0  
		BEACON\_INTERVAL (`CLOCK_SECOND*30`)  
		BEACON\_FORWARD\_DELAY (`random_rand() % (CLOCK_SECOND*4)`)  
		TOPOLOGY\_REPORT\_HOLD\_TIME (`CLOCK_SECOND*15`)  

	- `project-conf.h`:

		NETSTACK\_RDC contikimac\_driver  
		NETSTACK\_CONF\_RDC\_CHANNEL\_CHECK\_RATE 8  
		
- **07-contikimac-tr-cr-32**:

	- `my_collect.h`:

		TOPOLOGY\_REPORT 1  
		PIGGYBACKING 0  
		BEACON\_INTERVAL (`CLOCK_SECOND*30`)  
		BEACON\_FORWARD\_DELAY (`random_rand() % (CLOCK_SECOND*4)`)  
		TOPOLOGY\_REPORT\_HOLD\_TIME (`CLOCK_SECOND*15`)  

	- `project-conf.h`:

		NETSTACK\_RDC contikimac\_driver  
		NETSTACK\_CONF\_RDC\_CHANNEL\_CHECK\_RATE 32  
		
- **08-contikimac-tr-cr-32-1**:

	- `my_collect.h`:

		TOPOLOGY\_REPORT 1  
		PIGGYBACKING 0  
		BEACON\_INTERVAL (`CLOCK_SECOND*30`)  
		BEACON\_FORWARD\_DELAY (`random_rand() % (CLOCK_SECOND*4)`)  
		TOPOLOGY\_REPORT\_HOLD\_TIME (`CLOCK_SECOND*15`)  

	- `project-conf.h`:

		NETSTACK\_RDC contikimac\_driver  
		NETSTACK\_CONF\_RDC\_CHANNEL\_CHECK\_RATE 32  
		
