package martyr.replies;

import martyr.InCommand;

public class LUserMeReply extends GenericStringReply
{

public LUserMeReply()
{
}

public LUserMeReply( String string )
{
	super( string );
}

public String getIrcIdentifier()
{
	return "255";
}

public InCommand parse( String prefix, String identifier, String params )
{
	return new LUserMeReply( getParameter( params, 1 ) );
}

}

