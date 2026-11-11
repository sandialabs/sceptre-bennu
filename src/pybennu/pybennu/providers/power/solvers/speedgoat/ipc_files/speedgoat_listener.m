% speedgoat_listener.m
% Persistent MATLAB listener for Speedgoat control via file-based IPC with unique ID correlation
% Writes raw values/messages without ACK/ERR prefixes

commandFile = 'C:\MatlabIPC\command.txt';
resultFile = 'C:\MatlabIPC\result.txt';
commandLockFile = 'C:\MatlabIPC\command.lock';
resultLockFile = 'C:\MatlabIPC\result.lock';

diary('C:\MatlabIPC\matlab_diary.log');
diary on;

tg = slrealtime('TargetPC1');
tg.connect();

% Ensure no prior running models
tg.stop();

% Load and configure your model
fullModelPath = 'C:\MatlabIPC\speedgoat.slx';
[~, modelName, ~] = fileparts(fullModelPath);
load_system(fullModelPath);
set_param(modelName, 'SystemTargetFile', 'speedgoat.tlc');
set_param(modelName, 'StopTime', 'inf');

% Build and download the model
slbuild(modelName);
tg.load(modelName);

% Start the model on Speedgoat
tg.start();

% --- Create application and instrument subscribing to all signals ---
myApp = slrealtime.Application(modelName);

% 1. Query the compiled metadata for ALL available signals in the model
allSignalsMetadata = getSignals(myApp); 

hInst = slrealtime.Instrument(getApplicationFile(tg, modelName));
hInst.BufferData = true;

% 2. Subscribe to absolutely everything at startup
for i = 1:numel(allSignalsMetadata)
    blockPath = allSignalsMetadata(i).BlockPath;
    portIndex = allSignalsMetadata(i).PortIndex;
    hInst.addSignal(blockPath, portIndex);
end

tg.addInstrument(hInst);
% -----------------------------------------------------------------------

cleanupObj = onCleanup(@() cleanupSpeedgoat(tg));

fprintf('MATLAB listener started. Waiting for commands...\n');

% Build tag-to-block-path map (without port suffix)
blocks = find_system(modelName, 'Type', 'Block');
tagMap = containers.Map();

for i = 1:length(blocks)
    blk = blocks{i};
    [~, blkName, blkExt] = fileparts(blk);
    tag = [blkName blkExt];
    tagMap(tag) = blk;
end

% Helper function to build a map from signal keys to latest buffered values
function latestValuesMap = buildLatestValuesMap(dataChunk)
    latestValuesMap = containers.Map();
    keysList = keys(dataChunk);
    for i = 1:numel(keysList)
        key = keysList{i};
        dataEntry = dataChunk(key);
        if isstruct(dataEntry) && isfield(dataEntry, 'data')
            dataSamples = dataEntry.data;
            if ~isempty(dataSamples)
                latestValue = dataSamples(end);
            else
                latestValue = NaN; % No data available
            end
        else
            latestValue = NaN; % Unexpected format
        end
        latestValuesMap(key) = latestValue;
    end
end

keepRunning = true;
while keepRunning
    pause(0.1);

    % Acquire command lock (wait until lock file does not exist, then create it)
    if ~isfile(commandLockFile)
        % Create command lock file to acquire lock
        fidLock = fopen(commandLockFile, 'w');
        if fidLock == -1
            warning('Failed to create command lock file.');
            continue;
        end
        fwrite(fidLock, '1');
        fclose(fidLock);

        % Read all commands
        if isfile(commandFile)
            fid = fopen(commandFile, 'r');
            if fid == -1
                warning('Failed to open command file for reading.');
                delete(commandLockFile);
                continue;
            end
            commands = textscan(fid, '%s', 'Delimiter', '\n');
            fclose(fid);

            if isempty(commands{1})
                % No commands to process
                delete(commandLockFile);
                continue;
            end

            % Process first command
            cmd = strtrim(commands{1}{1});
            fprintf('Executing command: %s\n', cmd);

            % Rewrite command file without first line
            fid = fopen(commandFile, 'w');
            for iLine = 2:length(commands{1})
                fprintf(fid, '%s\n', commands{1}{iLine});
            end
            fclose(fid);

            % Release command lock
            delete(commandLockFile);

            % Parse command line: <ID> <ACTION> <TAG> [VALUE]
            tokens = regexp(cmd, '^(\S+)\s+(READ|WRITE|READMULTI)\s+(\S+)\s*(.*)$', 'tokens');
            if isempty(tokens)
                % Extract ID for error reporting
                idMatch = regexp(cmd, '^(\S+)', 'tokens');
                if ~isempty(idMatch)
                    id = idMatch{1}{1};
                else
                    id = 'UNKNOWN_ID';
                end
                resultStr = [id ' Invalid command format'];
            else
                tokens = tokens{1};
                id = tokens{1};
                action = tokens{2};
                tag = tokens{3};
                valStr = tokens{4};

                if strcmp(action, 'READ')
                    if isKey(tagMap, tag)
                        blkPath = tagMap(tag);
                        try
                            data = getsignal(tg, blkPath, 1);
                            if contains(lower(tag), '.status')
                                val = mat2str(logical(data));
                            else
                                val = num2str(data);
                            end                           
                            resultStr = [id ' ' val];
                        catch ME
                            resultStr = [id ' ' ME.message];
                        end
                    else
                        resultStr = [id ' Unknown tag'];
                    end

                elseif strcmp(action, 'READMULTI')
                    % Get buffered data and build latest values map
                    dataChunk = getBufferedData(hInst);
                    latestValuesMap = buildLatestValuesMap(dataChunk);

                    tagsList = strsplit(tag, ',');
                    values = cell(1, length(tagsList));

                    for k = 1:length(tagsList)
                        tagItem = tagsList{k};
                        if isKey(tagMap, tagItem)
                            blkPath = tagMap(tagItem);
                            % Append ':1' port suffix for lookup
                            fullKey = [blkPath ':1'];
                            if isKey(latestValuesMap, fullKey)
                                data = latestValuesMap(fullKey);
                                if contains(lower(tagItem), '.status')
                                    val = mat2str(logical(data));
                                else
                                    val = num2str(data);
                                end
                                values{k} = val;
                            else
                                values{k} = 'ERR:No buffered data';
                            end
                        else
                            values{k} = 'ERR:Unknown tag';
                        end
                    end

                    valStr = strjoin(values, ',');
                    resultStr = [id ' ACK=' valStr];

                elseif strcmp(action, 'WRITE')
                    if isKey(tagMap, tag)
                        blkPath = tagMap(tag);
                        try
                            if contains(lower(tag), '.status')
                                if strcmpi(valStr, 'true') || strcmp(valStr, '1')
                                    val = true;
                                elseif strcmpi(valStr, 'false') || strcmp(valStr, '0')
                                    val = false;
                                else
                                    error('Invalid boolean string: %s', valStr);
                                end 
                            else
                                val = str2double(valStr); 
                            end
                            setparam(tg, blkPath, 'Value', val);
                            resultStr = [id ' ACK=' num2str(val)];
                        catch ME
                            resultStr = [id ' ' ME.message];
                        end
                    else
                        resultStr = [id ' Unknown tag'];
                    end
                else
                    resultStr = [id ' Unknown action'];
                end
            end

            % Write result to result queue
            fprintf('Result: %s\n', resultStr);
            writeResult(resultStr);
        else
            % No command file, release lock
            delete(commandLockFile);
        end
    end
end

% Cleanup function called automatically on exit or Ctrl-C
function cleanupSpeedgoat(tg)
    try
        fprintf('Cleaning up: stopping and disconnecting Speedgoat target...\n');
        stop(tg);
        disconnect(tg);
        fprintf('Cleanup complete.\n');
    catch ME
        warning('Error during cleanup: %s', ME.message);
    end
end

% Helper function to write a line to result.txt with locking
function writeResult(line)
    persistent resultFile resultLockFile
    if isempty(resultFile)
        resultFile = 'C:\MatlabIPC\result.txt';
        resultLockFile = 'C:\MatlabIPC\result.lock';
    end

    % Wait for result lock to be free
    while isfile(resultLockFile)
        pause(0.01);
    end
    % Acquire result lock
    fidLock = fopen(resultLockFile, 'w');
    if fidLock == -1
        warning('Failed to create result lock file.');
        return;
    end
    fwrite(fidLock, '1');
    fclose(fidLock);

    % Append line to result file
    fid = fopen(resultFile, 'a');
    if fid == -1
        warning('Failed to open result file for appending.');
        delete(resultLockFile);
        return;
    end
    fprintf(fid, '%s\n', line);
    fclose(fid);

    % Release result lock
    delete(resultLockFile);
end
