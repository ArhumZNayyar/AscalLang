/*
 * AscalExecutor.cpp
 *
 *  Created on: Jan 11, 2021
 *      Author: andrewrubinstein
 */

#include "AscalExecutor.hpp"
#include "Keyword.hpp"


static const std::hash<std::string> hashfn;
static uint64_t hashFunctionCall(std::string &exp)
{
    return hashfn(exp);
}
static uint64_t h;
static uint64_t hashFunctionCall(uint64_t hash,AscalParameters& params)
{
	h = hash;
    for(Object *s:params)
    {
        hash += hashfn(s->getInstructions());
        hash ^= hash>>2;
        hash ^= hash<<5;
        hash ^= hash>>12;
        hash ^= hash<<18;
        hash ^= hash>>21;
        hash ^= hash<<26;
        hash ^= hash<<29;
    }
    return hash+h;
}
static uint64_t hash(AscalFrame<double>* currentFrame)
{
	uint64_t frameptr = (uint64_t) currentFrame;
	uint64_t rtnptr =   (uint64_t) currentFrame->returnPointer;
	uint64_t paramsptr = (uint64_t) currentFrame->getParamMemory();
	uint64_t locvarptr = (uint64_t) currentFrame->getLocalMemory();
	uint64_t hash = ((frameptr<<(1 + (15&rtnptr)))^currentFrame->memoPointer^paramsptr^locvarptr);
	hash ^= paramsptr>>16;
	hash ^= paramsptr<<16;
	hash ^= rtnptr>>6;
	hash ^= rtnptr<<26;
	hash ^= currentFrame->memoPointer>>3;
	hash ^= currentFrame->memoPointer<<20;
	hash ^= hash>>5;
	hash ^= hash<<10;
	hash ^= hash>>15;
	hash ^= hash<<20;
	hash ^= hash<<28;
	hash += frameptr<<8;
	hash += currentFrame->memoPointer;
	return hash;
}

template <class t>
static void printStack(stack<t> &operands)
{
    t and1;
    std::cout << "Stack: "<<std::endl;
    while(!operands.isEmpty())
    {
        operands.top(and1);
        operands.pop();
        std::cout << " "<<and1<<", ";
    }
    std::cout<<std::endl;
}

void AscalExecutor::addKeyWord(Keyword *key)
{
	inputMapper[key->getName()] = key;
}
void AscalExecutor::updateBoolSetting(AscalFrame<double>* frame)
	{

	    lastExp.pop();
	    SubStr id = ParsingUtil::getVarName(frame->exp,frame->index);
	    int endOfId = id.start+id.data.length();
	    while(frame->exp[endOfId] == ' ')
	    	endOfId++;
	    SubStr expression = ParsingUtil::getVarNameOrNum(frame->exp,endOfId);
	    short value = expression.data.length()>0?callOnFrame(frame, expression.data):-1;
	    setting<bool> set = boolsettings[id.data];
	    //inverts setting value via operator overloads of = and *
	    bool data = value<0?!*set:value;
	    setting<bool> newSetting(set.getName(),set.getCommand(),data);
	    boolsettings[set.getCommand()] = newSetting;
	    if(*boolsettings["o"])
	    {
	        std::cout<<set.getName()<<" Status: "<<data<<"\n";
	    }
	}
AscalExecutor::~AscalExecutor() {
	for(auto &[key,value]:inputMapper)
	{
		delete value;
	}
}

void AscalExecutor::loadFn(Object function)
{
        memory[function.id] = function;
        systemDefinedFunctions.push_back(function);
}

template <typename t>
void AscalExecutor::loadUserDefinedFn(Object &function, t &mem)
{


    if(!ParsingUtil::containsOperator(function.id))
    {
        mem[function.id] = function;
        if((void*)&mem == (void*)&memory)
        	userDefinedFunctions.push_back(function);
    }
    else
    {
        std::cerr<<"Error loading "<<function.id<<" due to the use of an operator in its name"<<std::endl;
        std::cerr<<"Operators: "<< '=' <<','<< '>' << ',' << '<' <<','<< '$' <<','<<
                'P' <<','<< '@' <<','<< '+' <<','<< '-' <<','<<
                '*'<<','<< '/' <<','<< '^' <<','<< '%' <<','<< 'C';
        throw std::string("Error Operator in Variable Name");
    }
}
void AscalExecutor::loadInitialFunctions()
{
    //Unary boolean Operations
    loadFn(Object("not","value=0",""));
    loadFn(Object("true","value=0=0",""));
    loadFn(Object("True","1",""));
    loadFn(Object("False","0",""));

    //Calculus Functions
    loadFn(Object("fprime","x*0+(f(x+0.000001)-f(x))/0.000001",""));

    //Trig Functions
    loadFn(Object("sinDeg","sin(toRad(theta))",""));
    loadFn(Object("csc","1/sin(theta)",""));
    loadFn(Object("cosDeg","cos(toRad(theta))",""));
    loadFn(Object("sec","1/cos(theta)",""));
    //Helpful functions
    loadFn(Object("fib","(x){loc counter = 0;loc first = 0;loc second = 1;loc int = 0;while counter<x{set int = second;set second = second+first;set first = int;set counter = counter+1;};first}",""));
    loadFn(Object("fibr","(x){loc fr = (x){when x > 1 then fr(x-1)+fr(x-2) else x end;};loc z = 0;memoize 1;set z = fr(x);memoize 0;z}",""));
    loadFn(Object("rfibr","(x){when x > 1 then rfibr(x-1)+rfibr(x-2) else x end;}",""));
    loadFn(Object("ack","when m=0 + n*0  then n+1 when n=0 then ack(m-1,1) when  m+n > 0 then ack(m-1,ack(m,n-1)) else 0 end",""));
    loadFn(Object("fastAck","(m,n){loc z = 0;memoize 1;set z = ack(m,n);memoize 0;z;}",""));
    loadFn(Object("gcd","(a,b){when b=0 then a when a=0=0 then gcd(b,a%b) else 0 end}",""));
    loadFn(Object("sumBetween","(numberzxa,numberzxb){"
            "when (numberzxb<numberzxa)+(numberzxb=numberzxa) then sumOneTo(numberzxa)-sumOneTo(numberzxb-1)"
            "else sumOneTo(numberzxb)-sumOneTo(numberzxa-1) end}"
            ,""));
    loadFn(Object("sumOneTo","(numberzxa*(numberzxa+1))/2",""));
    //factorial of >= 171 overflows double datatype
    loadFn(Object("rfact",
            "when (i>1)*(i<171) then rfact(i-1)*i when not(i>1) then 1 else 0 end",""));
    loadFn(Object("abs","when numberx<0 then numberx*-1 else numberx end",""));
    loadFn(Object("ln","when argument>0 then e@argument else null end",""));
    loadFn(Object("log","when argument>0 then 10@argument else null end",""));
    loadFn(Object("logbx","base@argument",""));
    loadFn(Object("sqrt","radicand^0.5",""));
    loadFn(Object("fact","when numberzxa<171 then numberzxaPnumberzxa when not(numberzxa<171) then 0 end",""));
    loadFn(Object("dist","sqrt((dx)^2+(dy)^2)",""));
    loadFn(Object("dist3d","sqrt((dx)^2+(dy)^2+(dz)^2)",""));
    loadFn(Object("toDeg","rad*180/pi",""));
    loadFn(Object("toRad","deg*pi/180",""));
    loadFn(Object("println","(x){loc counter = 0;while counter<x{set counter = counter +1;print \"endl\";};null",""));
    loadFn(Object("clear","println(150)",""));
    loadFn(Object("floor","x-x%1",""));
    loadFn(Object("ceiling","when x%1=0 then x else x+1-x%1 end",""));
    loadFn(Object("round","when x%1<0.5 then floor(x) else ceiling(x) end",""));

    //Stats Functions
    Object binProbDist("binprob","(total C events) * probabilityOfSuccess^events * (1-probabilityOfSuccess)^(total-events)","");
    loadFn(binProbDist);

    //Dietitian Functions
    Object eKCal("ekcal","(0*male*kg*cm*age*activity+ true(male)*5+not(male)*-161+(10*kg)+(6.25*cm)-(5*age))*activity","");
    loadFn(eKCal);

    //Constants Definition
    loadFn(Object("pi","3.141592653589793238462643383279",""));
    loadFn(Object("e","2.718281828459045",""));
    loadFn(Object("null",MAX,""));
    loadFn(Object("printf","print \"(x)endl\"",""));
}

double AscalExecutor::callOnFrame(AscalFrame<double>* callingFrame,const std::string &&subExp)
{
	return callOnFrame(callingFrame, subExp);
}
double AscalExecutor::callOnFrame(AscalFrame<double>* callingFrame,const std::string &subExp)
{
    ParamFrame<double> executionFrame(callingFrame->getParams(),callingFrame->getParamMemory(),callingFrame->getLocalMemory());
    executionFrame.returnPointer = callingFrame->returnPointer;
    executionFrame.exp = subExp;
    executionFrame.setIsDynamicAllocation(false);
    executionFrame.setContext(callingFrame->getContext());
    double data;
    try{
    	data = this->calculateExpression(&executionFrame);
    }catch(std::string &error)
    {
        throw error;
    }
    return data;
}
double AscalExecutor::callOnFrameFormatted(AscalFrame<double>* callingFrame,std::string subExp)
{
    ParamFrame<double> executionFrame(callingFrame->getParams(),callingFrame->getParamMemory(),callingFrame->getLocalMemory());
    executionFrame.exp = subExp;
    executionFrame.setIsDynamicAllocation(false);
    double data = calcWithOptions(&executionFrame);
    return data;
}
void AscalExecutor::createFrame(linkedStack<AscalFrame<double>* > &executionStack, AscalFrame<double>* currentFrame, Object *obj, int i,uint64_t hash)
{
    //save index
    currentFrame->index = i;
    if(ParsingUtil::isDouble(obj->getInstructions()))
    {
        int ib = 0;
        currentFrame->initialOperands.push(ParsingUtil::getNextDouble(obj->getInstructions(), ib));
        varCount++;
        if(*boolsettings["o"])
        	std::cout<<"Reading variable: "<<obj->id<<" = "<<obj->getInstructions()<<'\n';
    }
    else
    {
        frameCount++;
        if(*boolsettings["o"])
        {
        	std::stringstream data;
        	for(SubStr &s:obj->params)
        	{
        		data << s.data << ",";
        	}
        	std::cout<<"Parsing params then executing: "<<obj->id<<'('<<data.str()<<')'<<'\n';
        }
        //create and set new frame expression
        FunctionFrame<double> *newFrame = new FunctionFrame<double>(nullptr,nullptr,nullptr);
        newFrame->exp = obj->getInstructions();
        if(memory.count(obj->id) == 0)
        	(*newFrame->getParamMemory())[obj->id] = obj;
        newFrame->memoPointer = hash;
        //Create new frame, and set return pointer
        newFrame->returnPointer = currentFrame;
        //Context object ptr for this keyword resolution
        newFrame->setContext(obj);
        //push new frame
        executionStack.push(newFrame);
        for(int i = 0; i < obj->params.size(); i++)
        {
        	uint8_t startingIndex = 0;
        	while(obj->params[i].data[startingIndex++] == ' '){};
            allocated += sizeofFrame;
            if(obj->params[i].data[--startingIndex] != '&')
            {
                //create and set new frame expression
                ParamFrame<double>* pFrame = new ParamFrame<double>(currentFrame->getParams(),currentFrame->getParamMemory(),currentFrame->getLocalMemory());
                pFrame->exp = obj->params[i].data;
                //Create new frame, and set return pointer
                pFrame->returnPointer = newFrame;
                //Context object ptr for this keyword resolution
                pFrame->setContext(currentFrame->getContext());
                //push new frame
                executionStack.push(pFrame);
            }
            else
            {
                ParamFrameFunctionPointer<double>* pFrame = new ParamFrameFunctionPointer<double>(currentFrame->getParams(),currentFrame->getParamMemory(),currentFrame->getLocalMemory());
                uint32_t frameIndexBackup = currentFrame->index;
                SubStr startOfParam = ParsingUtil::getVarName(obj->params[i].data.substr(startingIndex), startingIndex);
                startOfParam.end = obj->params[i].start+startOfParam.data.size();
                pFrame->obj = resolveNextObjectExpression(currentFrame, startOfParam);
                currentFrame->index = frameIndexBackup;
                //Create new frame, and set return pointer
                pFrame->returnPointer = newFrame;
                //push new frame
                executionStack.push(pFrame);
            }
        }
    }
}
void AscalExecutor::clearStackOnError(bool printStack, std::string &error, linkedStack<AscalFrame<double>* > &executionStack, AscalFrame<double>* currentFrame, AscalFrame<double>* frame)
{
	frame->returnPointer = cachedRtnObject;
	        if(printStack)
	        {
	        	std::stringstream data;
	            size_t errorStartMarker = error.find("\1\1\2\3");
	            if(errorStartMarker<0 || errorStartMarker > error.length())
	            	errorStartMarker = 0;
	            data<<error.substr(0, errorStartMarker);
	            while(!executionStack.isEmpty())
	            {
	                executionStack.top(currentFrame);
	                	data << currentFrame->getType() << "   ~:"<<currentFrame->exp<<'\n';
	                if(currentFrame->isDynamicAllocation()){
	                    delete currentFrame;
	                }
	                executionStack.pop();
	            }
	            data << "\1\1\2\3";
	            data << error.substr(errorStartMarker);
	            error = data.str();
	        }
	        else
	        {
	            while(!executionStack.isEmpty())
	            {
	                executionStack.top(currentFrame);
	                if(currentFrame->isDynamicAllocation()){
	                    delete currentFrame;
	                }
	                executionStack.pop();
	            }
	        }
	        this->cachedRtnObject = nullptr;
}

double AscalExecutor::calculateExpression(AscalFrame<double>* frame)
{
    int statementCount = 1;
    //variable that is always a copy
    char peeker;
    //variables to hold the operands
    double and1,and2;
    //c is a variable to store each of the characters in the expression
    //in the for loop
    char currentChar;

    double data = 0;
    AscalFrame<double>* rtnptr = frame->returnPointer;
    frame->returnPointer = nullptr;
    //This loop handles parsing the numbers, and adding the data from the expression
    //to the stacks
    linkedStack<AscalFrame<double>* > executionStack;
    currentStack = &executionStack;
    AscalFrame<double>* currentFrame = frame;
    executionStack.push(currentFrame);
    try{
    while(!executionStack.isEmpty())
    {
        new_frame_execution:
        executionStack.top(currentFrame);
        if(currentFrame->isFunction() && currentFrame->isFirstRun())
        {
            currentFrame->setIsFirstRun(false);
        	if(*boolsettings["memoize"])
        	{
        		currentFrame->memoPointer = hashFunctionCall(currentFrame->memoPointer,*(currentFrame->getParams()));
        		if(memoPad.count(currentFrame->memoPointer))
        		{
        			data = memoPad[currentFrame->memoPointer];
        			currentFrame->returnResult(data, memory);
        			currentFrame->returnPointer = nullptr;
        			currentFrame->index = currentFrame->exp.length();
        			++rememberedFromMemoTableCount;

        			if(*boolsettings["o"])
        			{
        				if(std::to_string(data).length() != MAX.length())
        				{
        					std::cout<<"retrieving return value from memo tables\n";
        					std::cout<<"Returning value: "<<data<<'\n';
        				}
        			}
        		}
        	}

        }
        else if(currentFrame->isReturnFlagSet())
        {
    		if(currentFrame->isReturnObjectFlagSet())
        	{
        		Object *returnedObj = currentFrame->getReturnObject();
        		//accessing field in returned object
        		if(currentFrame->exp[currentFrame->index] == '.' || currentFrame->exp[currentFrame->index] == '[')
        		{
        			SubStr ptr("",0,currentFrame->index-1);
        			bool adjust = currentFrame->exp[currentFrame->index] == '.' || currentFrame->exp[currentFrame->index+1] == '&';
        			Object *objectResolved = resolveNextObjectExpression(currentFrame, ptr, returnedObj);
        			if(!returnedObj)
        				throw std::string("could not resolve field in returned object: "+returnedObj->id);
        			else
        				returnedObj = objectResolved;
        			currentFrame->index -= adjust;
        		}

        		uint32_t startOfparams = currentFrame->exp[currentFrame->index] =='('?
        			currentFrame->index:currentFrame->exp.size();
        		uint32_t endOfParams = returnedObj->setParams(currentFrame->exp, startOfparams);
                uint32_t startOfEnd = returnedObj->params.size()==0?currentFrame->index:currentFrame->index+endOfParams-1;
                //creates a new set of stack frames, one for the returned function/var and one for resolution,
                //and calculation of each of it's parameters
                //also returns function/var's value to this function's initialOperands stack
                createFrame(executionStack, currentFrame,
                    		returnedObj,startOfEnd,hashFunctionCall(returnedObj->id));
               	goto new_frame_execution;

        	}
        	else
        	{
        		currentFrame->setReturnFlag(false);
        	}
        }
        //If complex object return register is set on frame
        //we should internally have a pointer to an object that exists on the heap
        //so at the end of the process this needs to be cleaned
        //functioncall(&object.functioncall()) to handle this we need to be aware we are in a pfp frame
        //we need to alter the return value previously specified in pfp frame inside of a return keyword,
        //and set it equal to the obj returned which should be copied to the heap just in case it is
        //locally scoped in the function call
        //should be resolved in the return with the normal resolveNextObjectSafe return keyword must set rtn flag in parent frme

        //object.functioncall().fieldFromObjectReturnedByFnCall
        //object.functioncall().fieldThatIsAnFnFromObjectReturnedByFnCall()
        //object.functioncall[lookup]().fieldFromObjectReturnedByFnCall
        //object.functioncall[lookup]().fieldThatIsAnFnFromObjectReturnedByFnCall()
        //remainder after initial will be
        //.fieldFromObjectReturnedByFnCall or .fieldThatIsAnFnFromObjectReturnedByFnCall()
        for(int i = currentFrame->index;i <= currentFrame->exp.length();i++)
        {
         currentFrame->index = i;
         currentChar = currentFrame->exp[i];
         currentFrame->initialOperators.top(peeker);
         //anything used outside of the first { gets dropped from program stack, for param definition
         currentFrame->level += (currentChar == '{') - (currentChar == '}');


         if(currentChar == '{' && currentFrame->level == 1)
         {
            while(!currentFrame->initialOperands.isEmpty())
                currentFrame->initialOperands.pop();
            while(!currentFrame->initialOperators.isEmpty())
                currentFrame->initialOperators.pop();
         }
         else if(isalpha(currentChar) && (isalpha(frame->exp[i+1]) || !Calculator<double>::isOperator(currentChar)))
         {
        	 //This needs to be updated, and simplified it makes conditional jumps very expensive
             SubStr varName(ParsingUtil::getVarName(currentFrame->exp,i));

             //Keyword handling only one keyword at the beginning of each statement allowed,
             //including statements defined in variables

             if(inputMapper.count(varName.data) != 0)
             {

                 currentFrame->setAugmented(true);
                 bool isDynamicBackup = currentFrame->isDynamicAllocation();
                 std::string result;
                 if(!currentFrame->returnPointer)
                	 cachedRtnObject = rtnptr;
                 try{

                 result = inputMapper.find(varName.data)->second->action(currentFrame);
                 currentFrame->setIsDynamicAllocation(isDynamicBackup);
                 }catch(std::string& s){
                     currentFrame->setIsDynamicAllocation(isDynamicBackup);
                     throw s;
                 }

         	    cachedRtnObject = nullptr;

                 uint8_t subLevel = 0;
                 while((subLevel != 0 && currentFrame->exp[i]) || (currentFrame->exp[i] && currentFrame->exp[i] != ';' && currentFrame->exp[i] != '\n'))
                 {
                     i++;
                     subLevel += (currentFrame->exp[i]=='{') - (currentFrame->exp[i] == '}');

                 }
                 statementCount++;
                 if((currentFrame->exp[i] == 0 || currentFrame->exp[i+1] == 0) && result.length() == MAX.length())
                 {
                     currentFrame->exp = MAX;
                     if(currentFrame->initialOperands.size() != 0)
                         throw std::string("\nError called keyword: '"+varName.data+"'\nwithout statement separator ';' before previous expression.");
                 }
                 else if(isalpha(result[0]))
                 {
                     //for returning altered versions of the expression up till a statement separator like ;
                     //use cases: getting user input outside functions and variables
                     //when then when then end if else.
                     //while loops?
                     //just make sure first character in returned string is a throwaway alphabetical character
                     //it will not be parsed so ensure it is not meaningful code
                     currentFrame->exp = result.substr(1,currentFrame->exp.length());

                     i = -1;
                     currentFrame->index = 0;
                     continue;
                 }
                 else
                 {
                     currentFrame->exp = currentFrame->exp.substr(i,currentFrame->exp.length());
                 }
                 i = -1;
                 currentFrame->index = i;
             }
             else
             {
            	 uint32_t startOfparams = currentFrame->exp[varName.end+1] =='('?
                 varName.end+1:currentFrame->exp.size();
                 //Variable handling section
            	 currentFrame->index = varName.start;
            	 if(Object *data = resolveNextObjectExpression(currentFrame, varName))
                 {

                	 startOfparams = currentFrame->exp[varName.end+1] =='('?
                     varName.end+1:currentFrame->exp.size();
                     uint32_t endOfParams = data->setParams(currentFrame->exp, startOfparams);
                     uint32_t startOfEnd = varName.start+varName.data.size()-1+endOfParams;
                 //creates a new set of stack frames, one for the function/var and one for resolution,
                 //and calculation of each of it's parameters
                 //also returns function/var's value to this function's initialOperands stack
                	 createFrame(executionStack, currentFrame,
                                data,startOfEnd,hashFunctionCall(data->id));
                	 goto new_frame_execution;
                 }
                 else if(currentFrame->getParams()->getUseCount() < currentFrame->getParams()->size())
                 {

                	 if(currentFrame->exp[varName.end+1] == '.' || currentFrame->exp[varName.end+1] == '[')
                	 {
                		 std::stringstream error;
                		 error<<"Error while attempting to resolve parameter "<<varName.data<<" at position "<<ParsingUtil::to_string(varName.start)<<
                				 "\nfunction parameters must be defined when passing a reference to an object as parameter.\nTry wrapping the function in (";

                		 for(int i = 0;  i < currentFrame->getParams()->size();i++)
                			 error<<"<param"<<i+currentFrame->getParams()->getUseCount()+0<<">,";

                		 if(currentFrame->getParams()->size())
                			 error.seekp(-1,error.cur);
                		 error<<"){<original function body>}";
                		 error<<" to declare the function parameters";
                		 throw error.str();
                	 }
                     Object *paramVar = (*currentFrame->getParams())[currentFrame->getParams()->size() - 1 - currentFrame->getParams()->getUseCount()];
                     ++(*currentFrame->getParams());

                     Object *obj = ((*currentFrame->getParamMemory())[varName.data] = paramVar);
                     if(obj->id.size() == 0)
                     {
                    	 obj->id = varName.data;
                     }
                     uint32_t endOfParams = paramVar->setParams(currentFrame->exp, startOfparams);
                     uint32_t startOfEnd = varName.start+varName.data.length()-1+endOfParams;
                     createFrame(executionStack, currentFrame,
                    		 obj,startOfEnd,hashFunctionCall(paramVar->id));
                     goto new_frame_execution;
                 }
                 else
                 {
                    #if THROWERRORS == 1
                     if(ParsingUtil::cmpstr(varName.data,std::string("inf")))
                         throw std::string("Arithmetic Overflow in column: "+ParsingUtil::to_string(i));
                     else if(ParsingUtil::cmpstr(varName.data,std::string("nan")))
                         throw std::string("Error Undefined result calculated");
                     else
                         throw std::string("Invalid reference: "+varName.data+" in column: "+ParsingUtil::to_string(i));
                    #endif
                 }
             }

         }
         else
            if (currentChar==')')
            {
              currentFrame->initialOperators.top(peeker);
              while(!currentFrame->initialOperators.isEmpty() && peeker != '(')
              {
                  currentFrame->initialOperands.top(and1);
                  currentFrame->initialOperands.pop();
                processOperands.push(and1);
                processOperators.push(peeker);
                currentFrame->initialOperators.pop();
                currentFrame->initialOperators.top(peeker);
              }
              if(peeker != '(')
                  throw std::string("Error no opening parenthesis for closing ) at position: "+
                		  ParsingUtil::to_string(i+1));
              currentFrame->initialOperands.top(and2);
              currentFrame->initialOperands.pop();
              processOperands.push(and2);
              currentFrame->initialOperators.pop();
              //Send expression in parentheses to processStack for evaluation
              try{
                  and2 = processStack(processOperands,processOperators);
              }catch(std::string &error)
              {
                  throw std::string("In Expression: "+currentFrame->exp+" "+error);
              }

              currentFrame->initialOperands.push(and2);
            }
            //Section to parse numeric values from expression as a string to be inserted into
            //initialOperands stack
            else if(ParsingUtil::isNumeric(currentChar) ||
            (currentChar == '-' && ParsingUtil::isNumeric(currentFrame->exp[i+1]) &&
            		((i == 0 && !currentFrame->isAugmented()) ||
            				(i > 0 && (Calculator<double>::isNonParentheticalOperator(currentFrame->exp[i-1]) || currentFrame->exp[i-1] =='('))
            				))
				|| currentChar == '.')
            {
                double nextDouble = ParsingUtil::getNextDouble(currentFrame->exp,i);
                currentFrame->initialOperands.push(nextDouble);
            }
            else if(Calculator<double>::isOperator(currentChar))
            {
                currentFrame->initialOperators.push(currentChar);
            }
            else if(currentChar == ';')
            {
                currentFrame->clearStackIfAnotherStatementProceeds();
            }
          }
        //Finally pop all values off initial stack onto final stacks for processing
        while(!currentFrame->initialOperands.isEmpty() || !currentFrame->initialOperators.isEmpty())
        {
          if(!currentFrame->initialOperands.isEmpty())
          {
              currentFrame->initialOperands.top(and1);
              processOperands.push(and1);
              currentFrame->initialOperands.pop();
          }
          if(!currentFrame->initialOperators.isEmpty())
          {
              currentFrame->initialOperators.top(peeker);
              processOperators.push(peeker);
              currentFrame->initialOperators.pop();
          }
        }
        //process values in stacks, and return final solution
        data = processStack(processOperands, processOperators);
        if(currentFrame->getReturnPointer() && !currentFrame->getReturnPointer()->isReturnFlagSet())
        {
            if(currentFrame->isFunction())
            {
                if(*boolsettings["memoize"])
                {
                    memoPad[currentFrame->memoPointer] = data;
                }
                if(currentFrame != frame && *boolsettings["o"] && std::to_string(data).length() != MAX.length()){
                    std::cout<<"Returning value: "<<data<<'\n';
                }
            }
            currentFrame->returnResult(data, memory);
        }
        executionStack.pop();
        frame->returnPointer = rtnptr;
        cachedRtnObject = nullptr;
        if(currentFrame->isDynamicAllocation())
        {
            delete currentFrame;
        }
        currentFrame = nullptr;
    }

    }catch(std::string &error)
    {
    	clearStackOnError(true, error, executionStack, currentFrame, frame);
        throw error;
    }
    catch(int exitCode)
    {
    	clearStackOnError(false, frame->exp, executionStack, currentFrame, frame);
    	throw exitCode;
    }
    currentStack = nullptr;
    return data;
}

template <typename t>
void AscalExecutor::cleanOnError(bool timeInstruction, t start, t end)
{
	if(timeInstruction)
	    	        {
	    	            end = std::chrono::system_clock::now();

	    	            std::chrono::duration<double> elapsed_seconds = end-start;
	    	            std::time_t end_time = std::chrono::system_clock::to_time_t(end);

	    	            std::cout <<
	    	                      "Stack frames used: "<<frameCount<<'\n'
	    	                        <<"Variables accessed: "<<varCount<<'\n'
	    							<<"Values used from memo tables: "<<rememberedFromMemoTableCount<<'\n'
	    				<< "finished computation at " << std::ctime(&end_time)
	    				                      << "elapsed time: " << elapsed_seconds.count() << "s\n";
	    	        }

	    	    frameCount = 1;
	    	    varCount = 0;
	    	    rememberedFromMemoTableCount = 0;
	    	    savedOperands.clear();
	    	    savedOperands.clear();
	    	    processOperands.clear();
	    	    processOperators.clear();
	    	    memoPad.clear();
}

double AscalExecutor::calcWithOptions(AscalFrame<double>* frame)
{
    bool timeInstruction = *boolsettings["t"];

    std::chrono::system_clock::time_point start,end;
    if(timeInstruction){
    start = std::chrono::system_clock::now();
    }

    //alternate beginning of calculation for doubles
    //-------------------------
    double result;
    try{
    	result = calculateExpression(frame);
    }catch(std::string &error)
    {
    	cleanOnError(timeInstruction, start, end);
    	throw error;
    }catch(int exitCode){
    	cleanOnError(timeInstruction, start, end);
    	throw exitCode;
    }
    memoPad.clear();
    //------------------------
    {
        if(timeInstruction)
        {
            end = std::chrono::system_clock::now();

            std::chrono::duration<double> elapsed_seconds = end-start;
            std::time_t end_time = std::chrono::system_clock::to_time_t(end);

            std::cout <<
                      "Stack frames used: "<<frameCount<<'\n'
                        <<"Variables accessed: "<<varCount<<'\n'
						<<"Values used from memo tables: "<<rememberedFromMemoTableCount<<'\n'
			<< "finished computation at " << std::ctime(&end_time)
			                      << "elapsed time: " << elapsed_seconds.count() << "s\n";

        }
        if(std::to_string(result).length() != MAX.length() && *boolsettings["p"])
        {
        	if(*boolsettings["sci"])
        		std::cout<<result<<std::endl;
        	else
        		std::cout<<ParsingUtil::to_string(result)<<std::endl;
        }
    }
    frameCount = 1;
    varCount = 0;
    rememberedFromMemoTableCount = 0;
    savedOperands.clear();
    savedOperands.clear();
    processOperands.clear();
    processOperators.clear();
    return result;

}
double AscalExecutor::processStack(stack<double> &operands,stack<char> &operators)
{

#if THROWERRORS == 1
    if(operands.isEmpty() && operators.isEmpty()){}
    else if(operators.size() < operands.size()-1 ||
       operators.size() >= operands.size())
    {
        std::string error;
        if(operators.size() >= operands.size())
        {
            error = std::string("Error, too many operators.");
        }
        else
        {
            error = std::string("Error, too many operands.");
        }
        std::cerr<<"Operands ";
        printStack(operands);
        std::cerr<<"Operators ";
        printStack(operators);
        throw error;
    }
#endif
      double result = 0, and1 = 0, and2 = 0;
      char firstOperator, nextOperator;
      //loop to process expression saved in stack
     while(!operators.isEmpty() && operands.length()>1)
     {
    //initialize values so we can make sure no garbage, or previous values is in the variables
    //so top function can work properly
         nextOperator = 'a';
         firstOperator = 'a';
    //do while loop to search for operation with highest priority
      do
      {
          operands.top(and1);
          operands.pop();

          operands.top(and2);

          operators.top(firstOperator);
          operators.pop();

          operators.top(nextOperator);
          if(Calculator<double>::getPriority(nextOperator)>Calculator<double>::getPriority(firstOperator))
          {
              savedOperators.push(firstOperator);
              savedOperands.push(and1);
          }

      } while(Calculator<double>::getPriority(nextOperator)>Calculator<double>::getPriority(firstOperator));
      //perform found operation, and remove remaining operand from stack
      operands.pop();
      static Calculator<double> c;
      result = c.calc(firstOperator,and1,and2);
      operands.push(result);
      if(*boolsettings["o"])
        std::cout<<and1<<firstOperator<<and2<<" = "<<result<<'\n';
      //add overridden operators back to original stack
      while(!savedOperators.isEmpty())
      {
          savedOperators.top(firstOperator);
          savedOperators.pop();
          operators.push(firstOperator);
      }
      //replace overridden operands back in original stack
      while(!savedOperands.isEmpty())
      {
          savedOperands.top(and1);
          savedOperands.pop();
          operands.push(and1);
      }
    }

    //get result from processing expression
    operands.top(result);
    operands.pop();
      return result;
}

Object& AscalExecutor::getObject(AscalFrame<double>* frame, std::string &functionName)
{

	if(frame->getLocalMemory()->count(functionName))
	            {
	                return (*frame->getLocalMemory())[functionName];
	            }
	            else if(frame->getParamMemory()->count(functionName))
	            {
	                return *(*frame->getParamMemory())[functionName];
	            }
	            else if(memory.count(functionName))
	            {
	                return (memory)[functionName];
	            }
	            else
	            {
	                throw std::string("Error locating object "+functionName+"\n");
	            }
}
Object* AscalExecutor::getObjectNoError(AscalFrame<double>* frame, std::string &functionName)
{

	if(frame->getLocalMemory()->count(functionName))
	            {
	                return &(*frame->getLocalMemory())[functionName];
	            }
	            else if(frame->getParamMemory()->count(functionName))
	            {
	                return (*frame->getParamMemory())[functionName];
	            }
	            else if(memory.count(functionName))
	            {
	                return &(memory)[functionName];
	            }
	            else
	            {
	                return nullptr;
	            }
}

Object* AscalExecutor::resolveNextExprSafe(AscalFrame<double>* frame, SubStr &varName)
{
	Object* obj = AscalExecutor::resolveNextObjectExpression(frame, varName, nullptr);
	if(!obj)
	{
		throw std::string("Error Resolving object expression near index: "+ParsingUtil::to_string(varName.start)+" last parsed var name: "+varName.data);
	}
	return obj;
}
Object* AscalExecutor::resolveNextObjectExpression(AscalFrame<double>* frame, SubStr &varName, Object *obj)
{
	//getvarname
	frame->index = varName.end+1;
	if(!obj)
	{
		obj = getObjectNoError(frame, varName.data);
	}
	if(frame->getContext() && !obj &&
			varName.data.size() == 4 &&
					varName.data[0] == 't' && varName.data[1] == 'h' && varName.data[2] == 'i' && varName.data[3] == 's')
	{
		obj = frame->getContext()->getThis();
	}
	bool parsing;
	do {
		parsing = frame->exp[frame->index] == '.';
		while (obj && frame->exp.size() > frame->index && parsing)
		{
			varName = ParsingUtil::getVarName(frame->exp, frame->index);
			parsing = frame->exp[varName.end+1] == '.';
			frame->index = varName.end+1 + parsing;
			obj = &(*obj)[varName.data];
		}
		if(obj && frame->exp.size() > frame->index && frame->exp[frame->index] == '[')
		{
			//if index in array lookup
			if((ParsingUtil::isNumeric(frame->exp[++frame->index]) || isalpha(frame->exp[frame->index])) && frame->exp[frame->index])
			{
				int index = frame->index-1;
				SubStr lStr = ParsingUtil::getExprInString(frame->exp, index, '[',']',';');
				varName.data = lStr.data;
				double lookup = this->callOnFrame(frame, lStr.data);
				obj = &(*obj)[lookup];
				frame->index = index+1;
				varName.end = index+lStr.data.size()-4;
			}
			//else dictionary based lookup by string
			else if(frame->exp.size() > frame->index)
			{
				//By variable name representing string to lookup
				if(frame->exp[frame->index] == '&')
				{
					varName = ParsingUtil::getVarName(frame->exp, ++frame->index);
					frame->index = varName.start;
					obj = &(*obj)[AscalExecutor::resolveNextObjectExpression(frame, varName, nullptr)->listToString(memory)];
					varName.end++;
				}
				//By string literal
				else if(frame->exp[frame->index] == '\"')
				{
					varName = ParsingUtil::getVarName(frame->exp, ++frame->index);
					frame->index = varName.end+1;
					while(frame->index < frame->exp.size() && frame->exp[frame->index++] == ']'){}
					varName.end = frame->index ;
					obj = &(*obj)[varName.data];
				}
				else
				{
					throw std::string("Error invalid lookup parameters supplied to map: "+obj->id);
				}
			}
		}
		parsing = frame->exp[++frame->index] == '.' || frame->exp[frame->index] == '[';
	} while(obj && parsing);
	varName.start = varName.end - varName.data.size() + 1;
	return obj;
}
Object* AscalExecutor::resolveNextObjectExpressionPartial(AscalFrame<double>* frame, SubStr &varName, Object *obj)
{
	//getvarname
	frame->index = varName.end+1;
	if(!obj)
		obj = getObjectNoError(frame, varName.data);
	if((!obj && frame->getContext()) || (varName.data.size() == 4 && varName.data[0] == 't' && varName.data[1] == 'h' && varName.data[2] == 'i' && varName.data[3] == 's'))
		obj = frame->getContext()->getThis();
	bool parsing = frame->exp[frame->index] == '.';
	Object *possible = nullptr;
	do {
		parsing = frame->exp[frame->index] == '.';
		while (obj && frame->exp.size() > frame->index && parsing)
			{
				varName = ParsingUtil::getVarName(frame->exp, frame->index);
				parsing = frame->exp[varName.end+1] == '.';
				frame->index = varName.end+1 + parsing;
				if(parsing || frame->exp[varName.end+1] == '[')
					obj = &(*obj)[varName.data];
			}
			if(obj && frame->exp.size() > frame->index && frame->exp[frame->index] == '[')
			{
				//if index in array lookup
				if((ParsingUtil::isNumeric(frame->exp[++frame->index]) || isalpha(frame->exp[frame->index])) && frame->exp[frame->index])
				{
					int index = frame->index-1;
					varName = ParsingUtil::getExprInString(frame->exp, index, '[',']',';');
					varName.data = ParsingUtil::to_string(this->callOnFrame(frame, varName.data));
					frame->index = index+1;
				}
				//else dictionary based lookup by string
				else if(frame->exp.size() > frame->index)
				{
					//By variable name representing string to lookup
					if(frame->exp[frame->index] == '&')
					{
						varName = ParsingUtil::getVarName(frame->exp, ++frame->index);
						frame->index = varName.start;
						varName.data = AscalExecutor::resolveNextObjectExpression(frame, varName, nullptr)->listToString(memory);
						frame->index = varName.end+1;
						while(frame->index < frame->exp.size() && frame->exp[frame->index++]){}
						possible = &obj->getMapUnsafe(varName.data);
					}
					//By string literal
					else if(frame->exp[frame->index] == '\"')
					{
						varName = ParsingUtil::getVarName(frame->exp, ++frame->index);
						frame->index = varName.end+1;
						while(frame->index < frame->exp.size() && frame->exp[frame->index++] == ']'){}
						varName.end = frame->index - 1;
						possible = &obj->getMapUnsafe(varName.data);
					}
					else
					{
						throw std::string("Error invalid lookup parameters supplied to map: "+obj->id);
					}
				}
			}
			if(frame->exp[++frame->index] == '.' || frame->exp[frame->index] == '[')
			{
				parsing = true;
				obj = possible;
				possible = nullptr;
			}
	} while(obj && parsing);

	return obj;
}


