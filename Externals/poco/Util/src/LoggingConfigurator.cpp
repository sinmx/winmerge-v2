//
// LoggingConfigurator.cpp
//
// $Id: //poco/1.4/Util/src/LoggingConfigurator.cpp#1 $
//
// Library: Util
// Package: Configuration
// Module:  LoggingConfigurator
//
// Copyright (c) 2004-2006, Applied Informatics Software Engineering GmbH.
// and Contributors.
//
// Permission is hereby granted, free of charge, to any person or organization
// obtaining a copy of the software and accompanying documentation covered by
// this license (the "Software") to use, reproduce, display, distribute,
// execute, and transmit the Software, and to prepare derivative works of the
// Software, and to permit third-parties to whom the Software is furnished to
// do so, all subject to the following:
// 
// The copyright notices in the Software and this entire statement, including
// the above license grant, this restriction and the following disclaimer,
// must be included in all copies of the Software, in whole or in part, and
// all derivative works of the Software, unless such copies or derivative
// works are solely in the form of machine-executable object code generated by
// a source language processor.
// 
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE, TITLE AND NON-INFRINGEMENT. IN NO EVENT
// SHALL THE COPYRIGHT HOLDERS OR ANYONE DISTRIBUTING THE SOFTWARE BE LIABLE
// FOR ANY DAMAGES OR OTHER LIABILITY, WHETHER IN CONTRACT, TORT OR OTHERWISE,
// ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
// DEALINGS IN THE SOFTWARE.
//


#include "Poco/Util/LoggingConfigurator.h"
#include "Poco/Util/AbstractConfiguration.h"
#include "Poco/AutoPtr.h"
#include "Poco/Channel.h"
#include "Poco/FormattingChannel.h"
#include "Poco/Formatter.h"
#include "Poco/PatternFormatter.h"
#include "Poco/Logger.h"
#include "Poco/LoggingRegistry.h"
#include "Poco/LoggingFactory.h"
#include <map>


using Poco::AutoPtr;
using Poco::Formatter;
using Poco::PatternFormatter;
using Poco::Channel;
using Poco::FormattingChannel;
using Poco::Logger;
using Poco::LoggingRegistry;
using Poco::LoggingFactory;


namespace Poco {
namespace Util {


LoggingConfigurator::LoggingConfigurator()
{
}


LoggingConfigurator::~LoggingConfigurator()
{
}


void LoggingConfigurator::configure(AbstractConfiguration* pConfig)
{
	poco_check_ptr (pConfig);

	AutoPtr<AbstractConfiguration> pFormattersConfig(pConfig->createView("logging.formatters"));
	configureFormatters(pFormattersConfig);

	AutoPtr<AbstractConfiguration> pChannelsConfig(pConfig->createView("logging.channels"));
	configureChannels(pChannelsConfig);

	AutoPtr<AbstractConfiguration> pLoggersConfig(pConfig->createView("logging.loggers"));
	configureLoggers(pLoggersConfig);
}


void LoggingConfigurator::configureFormatters(AbstractConfiguration* pConfig)
{
	AbstractConfiguration::Keys formatters;
	pConfig->keys(formatters);
	for (AbstractConfiguration::Keys::const_iterator it = formatters.begin(); it != formatters.end(); ++it)
	{
		AutoPtr<AbstractConfiguration> pFormatterConfig(pConfig->createView(*it));
		AutoPtr<Formatter> pFormatter(createFormatter(pFormatterConfig));
		LoggingRegistry::defaultRegistry().registerFormatter(*it, pFormatter);
	}
}


void LoggingConfigurator::configureChannels(AbstractConfiguration* pConfig)
{
	AbstractConfiguration::Keys channels;
	pConfig->keys(channels);
	for (AbstractConfiguration::Keys::const_iterator it = channels.begin(); it != channels.end(); ++it)
	{
		AutoPtr<AbstractConfiguration> pChannelConfig(pConfig->createView(*it));
		AutoPtr<Channel> pChannel = createChannel(pChannelConfig);
		LoggingRegistry::defaultRegistry().registerChannel(*it, pChannel);
	}
	for (AbstractConfiguration::Keys::const_iterator it = channels.begin(); it != channels.end(); ++it)
	{
		AutoPtr<AbstractConfiguration> pChannelConfig(pConfig->createView(*it));
		Channel* pChannel = LoggingRegistry::defaultRegistry().channelForName(*it);
		configureChannel(pChannel, pChannelConfig);
	}
}


void LoggingConfigurator::configureLoggers(AbstractConfiguration* pConfig)
{
	typedef std::map<std::string, AutoPtr<AbstractConfiguration> > LoggerMap;

	AbstractConfiguration::Keys loggers;
	pConfig->keys(loggers);
	// use a map to sort loggers by their name, ensuring initialization in correct order (parents before children)
	LoggerMap loggerMap; 
	for (AbstractConfiguration::Keys::const_iterator it = loggers.begin(); it != loggers.end(); ++it)
	{
		AutoPtr<AbstractConfiguration> pLoggerConfig(pConfig->createView(*it));
		loggerMap[pLoggerConfig->getString("name", "")] = pLoggerConfig;
	}
	for (LoggerMap::iterator it = loggerMap.begin(); it != loggerMap.end(); ++it)
	{
		configureLogger(it->second);
	}
}


Formatter* LoggingConfigurator::createFormatter(AbstractConfiguration* pConfig)
{
	AutoPtr<Formatter> pFormatter(LoggingFactory::defaultFactory().createFormatter(pConfig->getString("class")));
	AbstractConfiguration::Keys props;
	pConfig->keys(props);
	for (AbstractConfiguration::Keys::const_iterator it = props.begin(); it != props.end(); ++it)
	{
		if (*it != "class")
			pFormatter->setProperty(*it, pConfig->getString(*it));		
	}
	return pFormatter.duplicate();
}


Channel* LoggingConfigurator::createChannel(AbstractConfiguration* pConfig)
{
	AutoPtr<Channel> pChannel(LoggingFactory::defaultFactory().createChannel(pConfig->getString("class")));
	AutoPtr<Channel> pWrapper(pChannel);
	AbstractConfiguration::Keys props;
	pConfig->keys(props);
	for (AbstractConfiguration::Keys::const_iterator it = props.begin(); it != props.end(); ++it)
	{
		if (*it == "pattern")
		{
			AutoPtr<Formatter> pPatternFormatter(new PatternFormatter(pConfig->getString(*it)));
			pWrapper = new FormattingChannel(pPatternFormatter, pChannel);
		}
		else if (*it == "formatter")
		{
			AutoPtr<FormattingChannel> pFormattingChannel(new FormattingChannel(0, pChannel));
			if (pConfig->hasProperty("formatter.class"))
			{
				AutoPtr<AbstractConfiguration> pFormatterConfig(pConfig->createView(*it));	
				AutoPtr<Formatter> pFormatter(createFormatter(pFormatterConfig));
				pFormattingChannel->setFormatter(pFormatter);
			}
			else pFormattingChannel->setProperty(*it, pConfig->getString(*it));
#if defined(__GNUC__) && __GNUC__ < 3
			pWrapper = pFormattingChannel.duplicate();
#else
			pWrapper = pFormattingChannel;
#endif
		}
	}
	return pWrapper.duplicate();
}


void LoggingConfigurator::configureChannel(Channel* pChannel, AbstractConfiguration* pConfig)
{
	AbstractConfiguration::Keys props;
	pConfig->keys(props);
	for (AbstractConfiguration::Keys::const_iterator it = props.begin(); it != props.end(); ++it)
	{
		if (*it != "pattern" && *it != "formatter" && *it != "class")
		{
			pChannel->setProperty(*it, pConfig->getString(*it));
		}
	}
}


void LoggingConfigurator::configureLogger(AbstractConfiguration* pConfig)
{
	Logger& logger = Logger::get(pConfig->getString("name", ""));
	AbstractConfiguration::Keys props;
	pConfig->keys(props);
	for (AbstractConfiguration::Keys::const_iterator it = props.begin(); it != props.end(); ++it)
	{
		if (*it == "channel" && pConfig->hasProperty("channel.class"))
		{
			AutoPtr<AbstractConfiguration> pChannelConfig(pConfig->createView(*it));	
			AutoPtr<Channel> pChannel(createChannel(pChannelConfig));
			configureChannel(pChannel, pChannelConfig);
			Logger::setChannel(logger.name(), pChannel);
		}
		else if (*it != "name")
		{
			Logger::setProperty(logger.name(), *it, pConfig->getString(*it));
		}
	}
}


} } // namespace Poco::Util
