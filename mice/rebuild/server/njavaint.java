/**
 * This class handles the java interview client.
 * @author Brandon Smith
 * @version 2.0
 */

import java.lang.*;
import java.net.*;
import java.util.*;
import java.io.*;

public class njavaint extends nbase
{
	public ainterview view;
	
	public njavaint(Socket gonnection, BufferedReader IN, OutputStreamWriter OUT)
	{
		super(gonnection,IN,OUT);
	}
	
	public void run()
	{
		if(!authenticate())
		{
			write("-ERR");
			closeConnection();
			return;
		}
		write("+OK");
		runloop();
		closeConnection();
	}
	
	/**
	 * This is what actually runs the thing.
	 * @author Brandon Smith
	 * @version 2.0
	 */
	public void runloop()
	{
		String in;
		while(true)
		{
			in = read();
			switch(in.charAt(0))
			{
			case 'q': //quit 
				return;
			case 'r': //reject
				view.reject();
				write(view.getQuestion());//this generates TWO read()s for the client
				break;
			case 's': //submit
				view.answer(read());
				write(view.getQuestion());//this generates TWO read()s for the client
				break;
			}
		}
	}
	
	/**
	 * This method pulls the initizization information from the client.
	 * @author Brandon Smith
	 * @version 2.0
	 * @return True when it works, false when it doesn't
	 */
	public boolean authenticate()
	{
		String mykey, rname, uname, pword;
		mykey = key.keygen();
		write(mykey);
		rname = read();
		uname = key.decrypt(mykey,read());
		pword = key.decrypt(mykey,read());
		view = new ainterview(rname);
		return view.intinit(imain.database,uname,pword);
	}
}