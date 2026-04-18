# CNCjs Pendant
## Install Dependencies
```bash
pip install keyboard pyjwt socketio
```
[//]: # (TODO: find correct socketio version)
## Finding your key

The program now **automatically finds and creates your key**. The following is only necessary when automatic key generation isn't working
<br/>
-
### Find your CNCjs secret
1. Find your .cncrc file (usually at ~/.cncrc or %USERPROFILE%\\.cncrc)
2. Find the line that says `"secret": "YOUR_SECRET_HERE"`
3. Copy the secret in the quotes
### Convert it to a key
```bash
NODE_PATH=$(npm root -g) node -e "const jwt = require('jsonwebtoken'); console.log(jwt.sign({ id: 'pendant', name: 'pendant' }, 'YOUR_SECRET_HERE', { expiresIn: '10y' }))"
```
### Use your key
Paste your generated key onto the line in the python program
```
CNCJS_TOKEN = 'YOUR_CNCJS_API_TOKEN_HERE'
```
