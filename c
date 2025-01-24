local function ModifyDecompiledCode(code)
	local function splitString(input, delimiter)
		local result = {}
		for match in input:gmatch("(.-)" .. delimiter) do
			table.insert(result, match)
		end
		if input:sub(-#delimiter) ~= delimiter then
			local lastSegment = input:match(".*" .. delimiter .. "(.*)")
			if lastSegment then
				table.insert(result, lastSegment)
			end
		end
		return result
	end

	local variableMapping = {}
	local serviceCounters = {}
	local classCounters = {}

	local function RenameVariables(line)

		local varName = line:match("local%s+(%w+)%s*=%s*{}") or line:match("local%s+(v_u_%d+)%s*=%s*{}") or line:match("local%s+(%w+)%s*=%s*{") or line:match("local%s+(v_u_%d+)%s*=%s*{")
		if varName then
			if not variableMapping[varName] then

				classCounters["class"] = (classCounters["class"] or 0) + 1
				local classCount = classCounters["class"]

				local newVarName = "class_" .. classCount
				variableMapping[varName] = newVarName
			end
			return line:gsub("local%s+" .. varName, "local " .. variableMapping[varName])
		end

		local varName = line:match("local%s+(v_u_%d+)%s*=")
		if varName and not line:match("game:GetService") and not line:match("require%((.-)%)")then
			local varNumber = tonumber(varName:match("v_u_(%d+)")) or (#variableMapping + 1)
			local newVarName = "var_" .. varNumber
			variableMapping[varName] = newVarName
			return line:gsub("local%s+" .. varName, "local " .. newVarName)
		end

		varName = line:match("local%s+(%w+)%s*=")
		if varName and not line:match("game:GetService") and not line:match("require%((.-)%)") then
			if not variableMapping[varName] then
				local newVarName = "var_" .. tostring(#variableMapping + 1)
				variableMapping[varName] = newVarName
			end
			return line:gsub("local%s+" .. varName, "local " .. variableMapping[varName])
		end

		if line:match("game:GetService") then
			local serviceName = line:match("game:GetService%(%\"(.-)%\"%)")
			varName = line:match("local%s+(%w+)%s*=") or line:match("local%s+(v_u_%d+)%s*=")
			if serviceName and varName and not line:match("require%((.-)%)") then
				serviceCounters[serviceName] = (serviceCounters[serviceName] or 0) + 1
				local serviceCount = serviceCounters[serviceName]
				local newVarName = "gs_" .. serviceName .. "_" .. (serviceCount - 1)
				variableMapping[varName] = newVarName
				return line:gsub("local%s+" .. varName, "local " .. newVarName)
			end
		end

		if line:match("require%((.-)%)") then
			local requirePath = line:match("require%((.-)%)")
			local varName = line:match("local%s+(%w+)%s*=") or line:match("local%s+(v_u_%d+)%s*=")
			if requirePath and varName then
				local memberName = line:match("%.([%w_]+)%s*$") or requirePath:match("([%w_]+)$")
				if memberName then
					local fullName = "r_" .. memberName .. "_0"
					variableMapping[varName] = fullName
					return line:gsub("local%s+" .. varName, "local " .. fullName)
				elseif line:match("WaitForChild%((.-)%)") then
					local memberName = line:match('WaitForChild%("(.+)"%)')
					local fullName = "r_" .. memberName .. "_0"
					variableMapping[varName] = fullName
					return line:gsub("local%s+" .. varName, "local " .. fullName)
				end
			end
		end

		return line
	end

	local lines = splitString(code, "\n")
	local modifiedLines = {}

	for _, line in ipairs(lines) do
		local modifiedLine = RenameVariables(line)

		for oldVar, newVar in pairs(variableMapping) do
			modifiedLine = modifiedLine:gsub("%f[%w_]" .. oldVar .. "%f[%W]", newVar)
		end

		table.insert(modifiedLines, modifiedLine)
	end

	if #modifiedLines == 0 then
		return code
	end

	return table.concat(modifiedLines, "\n")
end

local Decompile = function(Script)
	local bytecode = getscriptbytecode(Script)
	local encoded = crypt.base64.encode(bytecode)
	if bytecode then
		local Output = request({
			Url = "https://medal.hates.us/decompile",
			Method = "POST",
			Body = encoded,
		})
		task.wait(0.1)

		if Output.StatusCode == 200 then
			local success, modifiedCode = pcall(ModifyDecompiledCode, Output.Body)
			if success then
				local scriptType = Script.Name or "Unknown Type"
				local dateTime = os.date("%Y-%m-%d %H:%M:%S")
				return string.format(
					"--[[\n Decompiled by Medal.\n Output modified by Argon for improved readability and clarity.\n Script: %s\n Decompiled On: %s\n]]\n\n%s",
					scriptType,
					dateTime,
					modifiedCode
				)
			else
				return "--Decompiled by Medal\nFailed to modify code\nError:\n"  .. modifiedCode
			end
		end

		return "--Decompiled by Medal\nFailed to decompile bytecode\n"
	end
end
getgenv().decompile = Decompile
