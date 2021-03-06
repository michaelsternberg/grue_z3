! ----------------------------------------------------------------------------
!  PARSERM:  Core of parser.
!
!  Supplied for use with Inform 6                         Serial number 960912
!                                                                  Release 6/2
!  (c) Graham Nelson 1993, 1994, 1995, 1996 but freely usable (see manuals)
! ----------------------------------------------------------------------------

System_file;

Constant NULL $ffff;

! ----------------------------------------------------------------------------
!  Attribute and property definitions
!  The compass, directions, darkness and player objects
!  Definitions of fake actions
!  Library global variables
!  Private parser variables
!  Keyboard reading
!  Parser, level 0: outer shell, conversation, errors
!                1: grammar lines
!                2: tokens
!                3: object lists
!                4: scope and ambiguity resolving
!                5: object comparisons
!                6: word comparisons
!                7: reading words and moving tables about
!  Main game loop
!  Action processing
!  Menus
!  Time: timers and daemons
!  Considering light
!  Changing player personality
!  Printing short names
! ----------------------------------------------------------------------------

IFDEF MODULE_MODE;
Include "linklpa";
Constant DEBUG;
ENDIF;

Object GameController "GameController";

! ----------------------------------------------------------------------------
! Construct the compass - a dummy object containing the directions, which also
! represent the walls in whatever room the player is in (these are given the
! general-purpose "number" property for the programmer's convenience)
! ----------------------------------------------------------------------------

Object compass "compass" has concealed;

#IFNDEF WITHOUT_DIRECTIONS;
Object n_obj "north wall" compass      
  with name "n" "north" "wall",       article "the", door_dir n_to, number 0
  has  scenery;
Object s_obj "south wall" compass      
  with name "s" "south" "wall",       article "the", door_dir s_to, number 0
  has  scenery;
Object e_obj "east wall" compass      
  with name "e" "east" "wall",        article "the", door_dir e_to, number 0
   has  scenery;
Object w_obj "west wall" compass       
  with name "w" "west" "wall",        article "the", door_dir w_to, number 0
   has  scenery;
Object ne_obj "northeast wall" compass 
  with name "ne" "northeast" "wall",  article "the", door_dir ne_to, number 0
  has  scenery;
Object se_obj "southeast wall" compass
  with name "se" "southeast" "wall",  article "the", door_dir se_to, number 0
  has  scenery;
Object nw_obj "northwest wall" compass
  with name "nw" "northwest" "wall",  article "the", door_dir nw_to, number 0
  has  scenery;
Object sw_obj "southwest wall" compass
  with name "sw" "southwest" "wall",  article "the", door_dir sw_to, number 0
  has  scenery;
Object u_obj "ceiling" compass         
  with name "u" "up" "ceiling",       article "the", door_dir u_to, number 0
   has  scenery;
Object d_obj "floor" compass
  with name "d" "down" "floor",       article "the", door_dir d_to, number 0
   has  scenery;
Object out_obj "outside" compass
  with                                article "the", door_dir out_to, number 0
   has  scenery;
Object in_obj "inside" compass
  with                                article "the", door_dir in_to, number 0
   has  scenery;
#ENDIF;
! ----------------------------------------------------------------------------
! The other dummy object is "Darkness", not really a place but it has to be
! an object so that the name on the status line can be "Darkness":
! we also create the player object
! ----------------------------------------------------------------------------

Object thedark "Darkness"
  with initial 0,
       short_name "Darkness",
       description "It is pitch dark, and you can't see a thing.";

Object selfobj "yourself"
  with description "As good-looking as ever.", number 0,
       before $ffff, after $ffff, life $ffff, each_turn $ffff,
       time_out $ffff, describe $ffff, capacity 100,
       parse_name 0, short_name 0, orders 0,
  has  concealed animate proper transparent;

! ----------------------------------------------------------------------------
! Globals: note that the first one defined gives the status line place, the
! next two the score/turns
! ----------------------------------------------------------------------------

Global location = 1;
Global sline1 = 0;
Global sline2 = 0;

Global the_time = NULL;
Global time_rate = 1;
Global time_step = 0;

Global score = 0;
Global turns = 1;
Global player;

Global lightflag = 1;
Global real_location = thedark;
Global print_player_flag = 0;
Global deadflag = 0;

Global transcript_mode = 0;

Global last_score = 0;
Global notify_mode = 1;       ! Score notification

Global places_score = 0;
Global things_score = 0;
Global lookmode = 1;
Global lastdesc = 0;

Global top_object = 0;
Global standard_interpreter = 0;

! ----------------------------------------------------------------------------
! Parser variables accessible to the rest of the game
! ----------------------------------------------------------------------------

Array  inputobjs       --> 16;       ! To hold parameters
Global toomany_flag    = 0;          ! Flag for "take all made too many"
Global actor           = 0;          ! Person asked to do something
Global actors_location = 0;          ! Like location, but for the actor
Global action          = 0;          ! Thing he is asked to do
Global inp1            = 0;          ! First parameter
Global inp2            = 0;          ! Second parameter
Global special_number1 = 0;          ! First number, if one was typed
Global special_number2 = 0;          ! Second number, if two were typed
Global noun            = 0;          ! First noun
Global second          = 0;          ! Second noun
Array  multiple_object --> 64;       ! List of multiple parameters
Global special_word    = 0;          ! Dictionary address of "special"
Global special_number  = 0;          ! The number, if a number was typed
Global parsed_number   = 0;          ! For user-supplied parsing routines
global multiflag;                    ! Multiple-object flag
global notheld_mode  = 0;            ! To do with implicit taking
global onotheld_mode = 0;            !
global meta;                         ! Verb is a meta-command (such as "save")
global reason_code;                  ! Reason for calling a life
global consult_from;                 ! Word that "consult" topic starts on
global consult_words;                ! ...and number of words in topic

#IFV5;
global undo_flag = 0;                ! Can the interpreter provide "undo"?
#ENDIF;

global parser_trace = 0;             ! Set this to 1 to make the parser trace
                                     ! tokens and lines
global debug_flag = 0;               ! For debugging information
global lm_n;                         ! Parameters for LibraryMessages
global lm_o;

Constant REPARSE_CODE 10000;

Constant PARSING_REASON        0;
Constant TALKING_REASON        1;
Constant EACH_TURN_REASON      2;
Constant REACT_BEFORE_REASON   3;
Constant REACT_AFTER_REASON    4;
Constant LOOPOVERSCOPE_REASON  5;
Constant TESTSCOPE_REASON      6;

! ----------------------------------------------------------------------------
! The parser, beginning with variables private to itself:
! ----------------------------------------------------------------------------

Array  buffer2   string 120;    ! Buffers for supplementary questions
Array  parse2    string 64;     !
Array  parse3    string 64;     !

global wn;                      ! Word number (counts from 1)
global num_words;               ! Number of words typed
global verb_word;               ! Verb word (eg, take in "take all" or
                                ! "dwarf, take all") - address in dictionary
global verb_wordnum;            ! and the number in typing order (eg, 1 or 3)

global multi_mode;              ! Multiple mode
global multi_wanted;            ! Number of things needed in multitude
global multi_had;               ! Number of things actually found
global multi_context;           ! What token the multi-object was accepted for

Array pattern --> 8;            ! For the current pattern match
global pcount;                  ! and a marker within it
Array pattern2 --> 8;           ! And another, which stores the best match
global pcount2;                 ! so far

global parameters;              ! Parameters (objects) entered so far
global params_wanted;           ! Number needed (may change in parsing)

global nsns;                    ! Number of special_numbers entered so far

global inferfrom;               ! The point from which the rest of the
                                ! command must be inferred
global inferword;               ! And the preposition inferred

global oops_from = 0;           ! The "first mistake" point, where oops acts
global saved_oops = 0;          ! Used in working this out
Array  oops_heap --> 5;         ! Used temporarily by "oops" routine

Constant MATCH_LIST_SIZE 128;
Array  match_list    -> 128;
                                ! An array of matched objects so far
Array  match_classes -> 128;
                                ! An array of equivalence classes for them
global number_matched;          ! How many items in it?  (0 means none)
global number_of_classes;       ! How many equivalence classes?
global match_length;            ! How many typed words long are these matches?
global match_from;              ! At what word of the input do they begin?

global parser_action;           ! For the use of the parser when calling
global parser_one;              ! user-supplied routines
global parser_two;              !

global vague_word;              ! Records which vague word ("it", "them", ...)
                                ! caused an error
global vague_obj;               ! And what it was thought to refer to

global itobj=0;                 ! The object which is currently "it"
global himobj=0;                ! The object which is currently "him"
global herobj=0;                ! The object which is currently "her"

global lookahead;               ! The token after the object now being matched
global indef_mode;              ! "Indefinite" mode - ie, "take a brick" is in
                                ! this mode
global indef_type;              ! Bit-map holding types of specification
global indef_wanted;            ! Number of items wanted (100 for all)
global indef_guess_p;           ! Plural-guessing flag
global allow_plurals;           ! Whether they are presently allowed or not
global not_holding;             ! Object to be automatically taken as an
                                ! implicit command
Array  kept_results --> 16;     ! The delayed command (while the take happens)

global saved_wn;                ! These are temporary variables for Parser()
global saved_token;             ! (which hasn't enough spare local variables)

global held_back_mode = 0;      ! Flag: is there some input from last time
global hb_wn = 0;               ! left over?  (And a save value for wn)

global best_etype;              ! Error number used within parser
global nextbest_etype;          ! Error number used within parser
global etype;                   ! Error number used for individual lines

global last_command_from;       ! For sorting out "then again"
global last_command_to;         !

global token_was;               ! For noun filtering by user routines

global advance_warning;         ! What a later-named thing will be

global placed_in_flag;          ! To do with PlaceInScope
global length_of_noun;          ! Set by NounDomain to number of words in noun

global action_to_be;            ! So the parser can "cheat" in one case
global dont_infer;              ! Another dull flag

global scope_reason = PARSING_REASON;   ! For "each_turn" and reactions

global scope_token;             ! For scope:Routine tokens
global scope_error;
global scope_stage;

global ats_flag = 0;            ! For AddToScope routines
global ats_hls;                 !

global take_all_rule = 1;
global usual_grammar_after = 0;

#IFV5;
global just_undone = 0;         ! Can't have two successive UNDOs
#ENDIF;

Global pretty_flag=1;
Global menu_nesting = 0;

Global item_width=8;
Global item_name="Nameless item";
Global menu_item=0;

Global active_timers = 0;

! ----------------------------------------------------------------------------
!  Variables for Verblib
! ----------------------------------------------------------------------------

global inventory_stage = 1;
global c_style;
global lt_value;
global listing_together;
global listing_size;
global wlf_indent;
global inventory_style;
global keep_silent;
global receive_action;
#ifdef DEBUG;
global xcommsdir;
Global x_scope_count;
#endif;

! ----------------------------------------------------------------------------
!  The comma_word is a special word, used to substitute commas in the input
! ----------------------------------------------------------------------------

Constant comma_word 'xcomma';

! ----------------------------------------------------------------------------
!  In Advanced games only, the DrawStatusLine routine does just that: this is
!  provided explicitly so that it can be Replace'd to change the style, and
!  as written it emulates the ordinary Standard game status line, which is
!  drawn in hardware
! ----------------------------------------------------------------------------

#IFV5;
[ DrawStatusLine i width posa posb;
   @split_window 1; @set_window 1; @set_cursor 1 1; style reverse;
   width = 0->33; posa = width-26; posb = width-13;
   spaces (width-1);
   @set_cursor 1 2;  PrintShortName(location);
   if ((0->1)&2 == 0)
   {   if (width > 76)
       {   @set_cursor 1 posa; print "Score: ", sline1;
           @set_cursor 1 posb; print "Moves: ", sline2;
       }
       if (width > 63 && width <= 76)
       {   @set_cursor 1 posb; print sline1, "/", sline2;
       }
   }
   else
   {   @set_cursor 1 posa; print "Time: ";
       i=sline1%12; if (i<10) print " ";
       if (i==0) i=12;
       print i, ":";
       if (sline2<10) print "0";
       print sline2;
       if ((sline1/12) > 0) print " pm"; else print " am";
   }
   @set_cursor 1 1; style roman; @set_window 0;
];
#ENDIF;

! ----------------------------------------------------------------------------
!  The Keyboard routine actually receives the player's words,
!  putting the words in "a_buffer" and their dictionary addresses in
!  "a_table".  It is assumed that the table is the same one on each
!  (standard) call.
!
!  It can also be used by miscellaneous routines in the game to ask
!  yes-no questions and the like, without invoking the rest of the parser.
!
!  Return the number of words typed
! ----------------------------------------------------------------------------

[ Keyboard  a_buffer a_table  nw i w x1 x2;

    DisplayStatus();
    .FreshInput;

!  Save the start of the table, in case "oops" needs to restore it
!  to the previous time's table

    for (i=0:i<10:i++) oops_heap->i = a_table->i;

!  In case of an array entry corruption that shouldn't happen, but would be
!  disastrous if it did:

   a_buffer->0 = 120;
   a_table->0 = 64;

!  Print the prompt, and read in the words and dictionary addresses

    L__M(##Prompt);
    AfterPrompt();
    #IFV3; read a_buffer a_table; #ENDIF;
    temp_global = 0;
    #IFV5; read a_buffer a_table DrawStatusLine; #ENDIF;
    nw=a_table->1;

!  If the line was blank, get a fresh line
    if (nw == 0)
    { L__M(##Miscellany,10); jump FreshInput; }

!  Unless the opening word was "oops" or its abbreviation "o", return

    w=a_table-->1;
    if (w == #n$o or 'oops') jump DoOops;

#IFV5;
!  Undo handling

    if ((w == 'undo')&&(parse->1==1))
    {   if (turns==1)
        {   L__M(##Miscellany,11); jump FreshInput;
        }
        if (undo_flag==0)
        {   L__M(##Miscellany,6); jump FreshInput;
        }
        if (undo_flag==1) jump UndoFailed;
        if (just_undone==1)
        {   L__M(##Miscellany,12); jump FreshInput;
        }
        @restore_undo i;
        if (i==0)
        {   .UndoFailed;
            L__M(##Miscellany,7);
        }
        jump FreshInput;
    }
    @save_undo i;
    just_undone=0;
    undo_flag=2;
    if (i==-1) undo_flag=0;
    if (i==0) undo_flag=1;
    if (i==2)
    {   style bold;
        print (name) location, "^";
        style roman;
        L__M(##Miscellany,13);
        just_undone=1;
        jump FreshInput;
    }
#ENDIF;

    return nw;

    .DoOops;
    if (oops_from == 0)
    {   L__M(##Miscellany,14); jump FreshInput; }
    if (nw == 1)
    {   L__M(##Miscellany,15); jump FreshInput; }
    if (nw > 2)
    {   L__M(##Miscellany,16); jump FreshInput; }

!  So now we know: there was a previous mistake, and the player has
!  attempted to correct a single word of it.
!
!  Oops is very primitive: it gets the text buffer wrong, for instance.
!
!  Take out the 4-byte table entry for the supplied correction:
!  restore the 10 bytes at the front of the table, which were over-written
!  by what the user just typed: and then replace the oops_from word entry
!  with the correction one.
!
    x1=a_table-->3; x2=a_table-->4;
    for (i=0:i<10:i++) a_table->i = oops_heap->i;
    w=2*oops_from - 1;
    a_table-->w = x1;
    a_table-->(w+1) = x2;

    return nw;
];

Constant STUCK_PE     1;
Constant UPTO_PE      2;
Constant NUMBER_PE    3;
Constant CANTSEE_PE   4;
Constant TOOLIT_PE    5;
Constant NOTHELD_PE   6;
Constant MULTI_PE     7;
Constant MMULTI_PE    8;
Constant VAGUE_PE     9;
Constant EXCEPT_PE    10;
Constant ANIMA_PE     11;
Constant VERB_PE      12;
Constant SCENERY_PE   13;
Constant ITGONE_PE    14;
Constant JUNKAFTER_PE 15;
Constant TOOFEW_PE    16;
Constant NOTHING_PE   17;
Constant ASKSCOPE_PE  18;

! ----------------------------------------------------------------------------
!  The Parser routine is the heart of the parser.
!
!  It returns only when a sensible request has been made, and puts into the
!  "results" buffer:
!
!  Word 0 = The action number
!  Word 1 = Number of parameters
!  Words 2, 3, ... = The parameters (object numbers), but
!                    00 means "multiple object list goes here"
!                    01 means "special word goes here"
!
!  (Some of the global variables above are really local variables for this
!  routine, because the Z-machine only allows up to 15 local variables per
!  routine, and Parser runs out.)
!
!  To simplify the picture a little, a rough map of this routine is:
!
!  (A)    Get the input, do "oops" and "again"
!  (B)    Is it a direction, and so an implicit "go"?  If so go to (K)
!  (C)    Is anyone being addressed?
!  (D)    Get the verb: try all the syntax lines for that verb
!  (E)        Go through each token in the syntax line
!  (F)           Check (or infer) an adjective
!  (G)            Check to see if the syntax is finished, and if so return
!  (H)    Cheaply parse otherwise unrecognised conversation and return
!  (I)    Print best possible error message
!  (J)    Retry the whole lot
!  (K)    Last thing: check for "then" and further instructions(s), return.
!
!  The strategic points (A) to (K) are marked in the commentary.
!
!  Note that there are three different places where a return can happen.
!
! ----------------------------------------------------------------------------

[ Parser  results   syntax line num_lines line_address i j
                    token l m;

!  **** (A) ****

!  Firstly, in "not held" mode, we still have a command left over from last
!  time (eg, the user typed "eat biscuit", which was parsed as "take biscuit"
!  last time, with "eat biscuit" tucked away until now).  So we return that.

    if (notheld_mode==1)
    {   for (i=0:i<8:i++) results-->i=kept_results-->i;
        notheld_mode=0; rtrue;
    }

    if (held_back_mode==1)
    {   held_back_mode=0;
        for (i=0:i<64:i++) parse->i=parse2->i;
        new_line;
        jump ReParse;
    }

  .ReType;

    Keyboard(buffer,parse);

  .ReParse;

!  Initially assume the command is aimed at the player, and the verb
!  is the first word

    num_words=parse->1;
    wn=1;
    BeforeParsing();
    num_words=parse->1;
#ifdef DEBUG;
    if (parser_trace>=4)
    {   print "[ ", num_words, " to parse: ";
        for (i=1:i<=num_words:i++)
        {   j=parse-->((i-1)*2+1);
            if (j == 0) print "? ";
            else
            {   if (UnsignedCompare(j, 0-->4)>=0
                    && UnsignedCompare(j, 0-->2)<0) print (address) j;
                else print j; print " ";
            }
        }
        print "]^";
    }
#endif;
    verb_wordnum=1;
    actor=player; actors_location=location;
    usual_grammar_after = 0;

  .AlmostReParse;

    token_was = 0; ! In case we're still in "user-filter" mode from last round
    scope_token = 0;
    action_to_be = NULL;

!  Begin from what we currently think is the verb word

  .BeginCommand;
    wn=verb_wordnum;
    verb_word = NextWordStopped();

!  If there's no input here, we must have something like
!  "person,".

    if (verb_word==-1)
    {   best_etype = STUCK_PE; jump GiveError; }

!  Now try for "again" or "g", which are special cases:
!  don't allow "again" if nothing has previously been typed;
!  simply copy the previous parse table and ReParse with that

    if (verb_word==#n$g) verb_word='again';
    if (verb_word=='again')
    {   if (actor~=player)
        {   print "To repeat a command like ~frog, jump~, just say \
             ~again~, not ~frog, again~.^"; jump ReType; }
        if (parse3->1==0)
        {   print "You can hardly repeat that.^"; jump ReType; }
        for (i=0:i<64:i++) parse->i=parse3->i;
        jump ReParse;
    }

!  Save the present parse table in case of an "again" next time

    if (verb_word~='again')
        for (i=0:i<64:i++)
            parse3->i=parse->i;

    if (usual_grammar_after==0)
    {   i = RunRoutines(actor, grammar);
#ifdef DEBUG;
        if (parser_trace>=2 && actor.grammar~=0 or NULL)
            print " [Grammar property returned ", i, "]^";
#endif;
        if (i<0) { usual_grammar_after = verb_wordnum; i=-i; }
        if (i==1)
        {   results-->0 = action;
            results-->1 = noun;
            results-->2 = second;
            rtrue;
        }
        if (i~=0) { verb_word = i; wn--; verb_wordnum--; }
        else
        {   wn = verb_wordnum; verb_word=NextWord();
        }
    }
    else usual_grammar_after=0;

!  **** (B) ****

!  If the first word is not listed as a verb, it must be a direction
!  or the name of someone to talk to
!  (NB: better avoid having a Mr Take or Mrs Inventory around...)

    if (verb_word==0 || ((verb_word->#dict_par1) & 1) == 0)
    {   

!  So is the first word an object contained in the special object "compass"
!  (i.e., a direction)?  This needs use of NounDomain, a routine which
!  does the object matching, returning the object number, or 0 if none found,
!  or REPARSE_CODE if it has restructured the parse table so that the whole parse
!  must be begun again...

        wn=verb_wordnum;
        l=NounDomain(compass,0,0); if (l==REPARSE_CODE) jump ReParse;

!  If it is a direction, send back the results:
!  action=GoSub, no of arguments=1, argument 1=the direction.

        if (l~=0)
        {   results-->0 = ##Go;
            results-->1 = 1;
            results-->2 = l;
            jump LookForMore;
        }

!  **** (C) ****

!  Only check for a comma (a "someone, do something" command) if we are
!  not already in the middle of one.  (This simplification stops us from
!  worrying about "robot, wizard, you are an idiot", telling the robot to
!  tell the wizard that she is an idiot.)

        if (actor==player)
        {   for (j=2:j<=num_words:j++)
            {   i=NextWord(); if (i==comma_word) jump Conversation;
            }

            verb_word=UnknownVerb(verb_word);
            if (verb_word~=0) jump VerbAccepted;
        }

        best_etype=VERB_PE; jump GiveError;

!  NextWord nudges the word number wn on by one each time, so we've now
!  advanced past a comma.  (A comma is a word all on its own in the table.)

      .Conversation;
        j=wn-1;
        if (j==1) { print "You can't begin with a comma.^"; jump ReType; }

!  Use NounDomain (in the context of "animate creature") to see if the
!  words make sense as the name of someone held or nearby

        wn=1; lookahead=1;
        scope_reason = TALKING_REASON;
        l=NounDomain(player,actors_location,6);
        scope_reason = PARSING_REASON;
        if (l==REPARSE_CODE) jump ReParse;

        if (l==0) { print "You seem to want to talk to someone, \
                         but I can't see whom.^"; jump ReType; }

!  The object addressed must at least be "talkable" if not actually "animate"
!  (the distinction allows, for instance, a microphone to be spoken to,
!  without the parser thinking that the microphone is human).

        if (l hasnt animate && l hasnt talkable)
        {   print "You can't talk to "; DefArt(l); print ".^"; jump ReType; }

!  Check that there aren't any mystery words between the end of the person's
!  name and the comma (eg, throw out "dwarf sdfgsdgs, go north").

        if (wn~=j)
        {   print "To talk to someone, try ~someone, hello~ or some such.^";
            jump ReType;
        }

!  The player has now successfully named someone.  Adjust "him", "her", "it":

        ResetVagueWords(l);

!  Set the global variable "actor", adjust the number of the first word,
!  and begin parsing again from there.

        verb_wordnum=j+1; actor=l;
        actors_location=l;
        while (parent(actors_location)~=0)
            actors_location=parent(actors_location);
#ifdef DEBUG;
        if (parser_trace>=1)
            print "[Actor is ", (the) actor, " in ",
                (name) actors_location, "]^";
#endif;
        jump BeginCommand;
    }

!  **** (D) ****

   .VerbAccepted;

!  We now definitely have a verb, not a direction, whether we got here by the
!  "take ..." or "person, take ..." method.  Get the meta flag for this verb:

    meta=((verb_word->#dict_par1) & 2)/2;

!  You can't order other people to "full score" for you, and so on...

    if (meta==1 && actor~=player)
    {   best_etype=VERB_PE; meta=0; jump GiveError; }

!  Now let i be the corresponding verb number, stored in the dictionary entry
!  (in a peculiar 255-n fashion for traditional Infocom reasons)...

    i=$ff-(verb_word->#dict_par2);

!  ...then look up the i-th entry in the verb table, whose address is at word
!  7 in the Z-machine (in the header), so as to get the address of the syntax
!  table for the given verb...

    syntax=(0-->7)-->i;

!  ...and then see how many lines (ie, different patterns corresponding to the
!  same verb) are stored in the parse table...

    num_lines=(syntax->0)-1;

!  ...and now go through them all, one by one.
!  To prevent vague_word 0 being misunderstood,

   vague_word='it'; vague_obj=itobj;

#ifdef DEBUG;
   if (parser_trace>=1)
   {    print "[Parsing for the verb '", (address) verb_word,
              "' (", num_lines+1, " lines)]^";
   }
#endif;

   best_etype=STUCK_PE; nextbest_etype=best_etype;
!  "best_etype" is the current failure-to-match error - it is by default
!  the least informative one, "don't understand that sentence"


!  **** (E) ****

    for (line=0:line<=num_lines:line++)
    {   line_address = syntax+1+line*8;

#ifdef DEBUG;
        if (parser_trace>=1)
        {   print "[Line ", line, ": ", line_address->0, " parameters: ";
            for (pcount=1:pcount<=6:pcount++)
            {   token=line_address->pcount;
                print token, " ";
            }
            print " -> action ", line_address->7, "]^";
        }
#endif;

!  We aren't in "not holding" or inferring modes, and haven't entered
!  any parameters on the line yet, or any special numbers; the multiple
!  object is still empty.

        not_holding=0;
        inferfrom=0;
        parameters=0;
        params_wanted = line_address->0;
        nsns=0; special_word=0; special_number=0;
        multiple_object-->0 = 0;
        multi_context = 0;
        etype=STUCK_PE;
        action_to_be = line_address->7;

!  Put the word marker back to just after the verb

        wn=verb_wordnum+1;

!  An individual "line" contains six tokens...  There's a preliminary pass
!  first, to parse late tokens early if necessary (because of mi or me).
!  We also check to see whether the line contains any "multi"s.

        advance_warning=-1; indef_mode=0;
        for (i=0,m=0,pcount=1:pcount<=6:pcount++)
        {   scope_token=0;
            token=line_address->pcount;
            if (token==2) m++;
            if (token<180) i++;
            if (token==4 or 5 && i==1)
            {
#ifdef DEBUG;
                if (parser_trace>=2) print " [Trying look-ahead]^";
#endif;
                pcount++;
                while (pcount<=6 && line_address->pcount>=180) pcount++;
                token=line_address->(pcount-1);
                if (token>=180)
                {   j=AdjectiveAddress(token);

                    !  Now look for word with j, move wn, parse next
                    !  token...
                    while (wn <= num_words)
                    {   if (NextWord()==j)
                        {   l = NounDomain(actors_location,actor,token);
#ifdef DEBUG;
                            if (parser_trace>=2)
                            {   print " [Forward token parsed: ";
                                if (l==REPARSE_CODE) print "re-parse request]^";
                                if (l==1) print "but multiple found]^";
                                if (l==0) print "hit error ", etype, "]^";
                            }
#endif;
                            if (l==REPARSE_CODE) jump ReParse;
                            if (l>=2)
                            {   advance_warning = l;
#ifdef DEBUG;
                                if (parser_trace>=3)
                                {   DefArt(l); print "]^";
                                }
#endif;
                            }
                        }
                    }
                }
            }
        }

!  Slightly different line-parsing rules will apply to "take multi", to
!  prevent "take all" behaving correctly but misleadingly when there's
!  nothing to take.

        take_all_rule = 0;
        if (m==1 && params_wanted==1 && action_to_be==##Take)
            take_all_rule = 1;

!  And now start again, properly, forearmed or not as the case may be.

        not_holding=0;
        inferfrom=0;
        parameters=0;
        nsns=0; special_word=0; special_number=0;
        multiple_object-->0 = 0;
        etype=STUCK_PE;
        action_to_be = line_address->7;
        wn=verb_wordnum+1;

!  "Pattern" gradually accumulates what has been recognised so far,
!  so that it may be reprinted by the parser later on

        for (pcount=1:pcount<=6:pcount++)
        {   pattern-->pcount=0; scope_token=0;

            token=line_address->pcount;

#ifdef DEBUG;
            if (parser_trace>=2)
            {   print " [Token ",pcount, " is ", token, ": ";
                if (token<16)
                {   if (token==0) print "<noun> or null";
                    if (token==1) print "<held>";
                    if (token==2) print "<multi>";
                    if (token==3) print "<multiheld>";
                    if (token==4) print "<multiexcept>";
                    if (token==5) print "<multiinside>";
                    if (token==6) print "<creature>";
                    if (token==7) print "<special>";
                    if (token==8) print "<number>";
                }
                if (token>=16 && token<48)
                    print "<noun filter by routine ",token-16, ">";
                if (token>=48 && token<80)
                    print "<general parse by routine ",token-48, ">";
                if (token>=80 && token<128)
                    print "<scope parse by routine ",token-80, ">";
                if (token>=128 && token<180)
                    print "<noun filter by attribute ",token-128, ">";
                if (token>180)
                {   print "<adjective ",255-token, " '",
                    (address) AdjectiveAddress(token), "'>";
                }
                print " at word number ", wn, "]^";
            }
#endif;

!  Lookahead is set to the token after this one, or 8 if there isn't one.
!  (Complicated because the line is padded with 0's.)

            m=pcount+1; lookahead=8;
            if (m<=6) lookahead=line_address->m;
            if (lookahead==0)
            {   m=parameters; if (token<=7) m++;
                if (m>=params_wanted) lookahead=8;
            }

!  **** (F) ****

!  When the token is a large number, it must be an adjective:
!  remember the adjective number in the "pattern".

            if (token>180)
            {   pattern-->pcount = REPARSE_CODE+token;

!  If we've run out of the player's input, but still have parameters to
!  specify, we go into "infer" mode, remembering where we are and the
!  adjective we are inferring...

                if (wn > num_words)
                {   if (inferfrom==0 && parameters<params_wanted)
                    { inferfrom=pcount; inferword=token; }

!  Otherwise, this line must be wrong.

                    if (inferfrom==0) break;
                }

!  Whereas, if the player has typed something here, see if it is the
!  required adjective... if it's wrong, the line must be wrong,
!  but if it's right, the token is passed (jump to finish this token).

                if (wn <= num_words && token~=AdjectiveWord()) break;
                jump TokenPassed;
            }

!  **** (G) ****
!  Check now to see if the player has entered enough parameters...
!  (since params_wanted is the number of them)

            if (parameters == params_wanted)
            {  

!  If the player has entered enough parameters already but there's still
!  text to wade through: store the pattern away so as to be able to produce
!  a decent error message if this turns out to be the best we ever manage,
!  and in the mean time give up on this line

!  However, if the superfluous text begins with a comma, "and" or "then" then
!  take that to be the start of another instruction

                if (wn <= num_words)
                {   l=NextWord();
                    if (l=='then' or comma_word)
                    {   held_back_mode=1; hb_wn=wn-1; }
                    else
                    {   for (m=0:m<8:m++) pattern2-->m=pattern-->m;
                        pcount2=pcount;
                        etype=UPTO_PE; break;
                    }
                }

!  Now, we may need to revise the multiple object because of the single one
!  we now know (but didn't when the list was drawn up).

                if (parameters>=1 && results-->2 == 0)
                {   l=ReviseMulti(results-->3);
                    if (l~=0) { etype=l; break; }
                }
                if (parameters>=2 && results-->3 == 0)
                {   l=ReviseMulti(results-->2);
                    if (l~=0) { etype=l; break; }
                }

!  To trap the case of "take all" inferring only "yourself" when absolutely
!  nothing else is in the vicinity...

                if (take_all_rule==2 && results-->2 == actor)
                {   best_etype = NOTHING_PE; jump GiveError;
                }

#ifdef DEBUG;
                if (parser_trace>=1)
                    print "[Line successfully parsed]^";
#endif;

!  At this point the line has worked out perfectly, and it's a matter of
!  sending the results back...
!  ...pausing to explain any inferences made (using the pattern)...

                if (inferfrom~=0)
                {   print "("; PrintCommand(inferfrom,1); print ")^";
                }

!  ...and to copy the action number, and the number of parameters...

                results-->1 = params_wanted;
                results-->0 = line_address->7;

!  ...and to reset "it"-style objects to the first of these parameters, if
!  there is one (and it really is an object)...

                if (parameters > 0 && results-->2 >= 2)
                    ResetVagueWords(results-->2);

!  ...and declare the user's input to be error free...

                oops_from = 0;

!  ...and worry about the case where an object was allowed as a parameter
!  even though the player wasn't holding it and should have been: in this
!  event, keep the results for next time round, go into "not holding" mode,
!  and for now tell the player what's happening and return a "take" request
!  instead...

                if (not_holding~=0 && actor==player)
                {   notheld_mode=1;
                    for (i=0:i<8:i++) kept_results-->i = results-->i;
                    results-->0 = ##Take;
                    results-->1 = 1;
                    results-->2 = not_holding;
                    print "(first taking "; DefArt(not_holding); print ")^";
                }

!  (Notice that implicit takes are only generated for the player, and not
!  for other actors.  This avoids entirely logical, but misleading, text
!  being printed.)

!  ...and finish.

                if (held_back_mode==1) { wn=hb_wn; jump LookForMore; }
                rtrue;
            }

!  Otherwise, the player still has at least one parameter to specify: an
!  object of some kind is expected, and this we hand over to POL.

            if (token==6 && (action_to_be==##Answer or ##Ask or ##AskFor
                             || action_to_be==##Tell))
                scope_reason=TALKING_REASON;
            l=ParseObjectList(results,token);
            scope_reason=PARSING_REASON;
#ifdef DEBUG;
            if (parser_trace>=3)
            {   print "  [Parse object list replied with";
                if (l==REPARSE_CODE) print " re-parse request]^";
                if (l==0) print " token failed, error type ", etype, "]^";
                if (l==1) print " token accepted]^";
            }
#endif;
            if (l==REPARSE_CODE) jump ReParse;
            if (l==0)    break;

!  The token has been successfully passed; we are ready for the next.

            .TokenPassed;
        }

!  But if we get here it means that the line failed somewhere, so we continue
!  the outer for loop and try the next line...

        if (etype>best_etype)
        {   best_etype=etype;
        }
        if (etype~=ASKSCOPE_PE && etype>nextbest_etype)
        {   nextbest_etype=etype;
        }

!  ...unless the line was something like "take all" which failed because
!  nothing matched the "all", in which case we stop and give an error now.

        if (take_all_rule == 2 && etype==NOTHING_PE) break;
   }

!  So that if we get here, each line for the specified verb has failed.

!  **** (H) ****

  .GiveError;
        etype=best_etype;

!  Errors are handled differently depending on who was talking.

!  If the command was addressed to somebody else (eg, "dwarf, sfgh") then
!  it is taken as conversation which the parser has no business in disallowing.

    if (actor~=player)
    {   if (usual_grammar_after>0)
        {   verb_wordnum = usual_grammar_after;
            jump AlmostReParse;
        }
        wn=verb_wordnum;
        special_word=NextWord();
        if (special_word=='xcomma')
        {   special_word=NextWord();
            verb_wordnum++;
        }
        special_number=TryNumber(verb_wordnum);
        results-->0=##NotUnderstood;
        results-->1=2;
        results-->2=1; special_number1=special_word;
        results-->3=actor;
        consult_from = verb_wordnum; consult_words = num_words-consult_from+1;
        rtrue;
    }

!  **** (I) ****

!  If the player was the actor (eg, in "take dfghh") the error must be printed,
!  and fresh input called for.  In three cases the oops word must be jiggled.

    if (ParserError(etype)~=0) jump ReType;

    if (etype==STUCK_PE)
             {   print "I didn't understand that sentence.^"; oops_from=1; }
    if (etype==UPTO_PE)
             {   print "I only understood you as far as wanting to ";
                 for (m=0:m<8:m++) pattern-->m = pattern2-->m;
                 pcount=pcount2; PrintCommand(0,1); print ".^";
             }
    if (etype==NUMBER_PE)
                 print "I didn't understand that number.^";
    if (etype==CANTSEE_PE)
!             {   print "You can't see any such thing.^";
             {   print "There's nothing like that here.^";
                 oops_from=saved_oops; }
    if (etype==TOOLIT_PE)
                 print "You seem to have said too little!^";
    if (etype==NOTHELD_PE)
             {   print "You aren't holding that!^";
                 oops_from=saved_oops; }
    if (etype==MULTI_PE)
                 print "You can't use multiple objects with that verb.^";
    if (etype==MMULTI_PE)
                 print "You can only use multiple objects once on a line.^";
    if (etype==VAGUE_PE)
             {   print "I'm not sure what ~", (address) vague_word,
                       "~ refers to.^"; }
    if (etype==EXCEPT_PE)
                 print "You excepted something not included anyway!^";
    if (etype==ANIMA_PE)
                 print "You can only do that to something animate.^";
    if (etype==VERB_PE)
                 print "That's not a verb I recognise.^";
    if (etype==SCENERY_PE)
                 print "That's not something you need to refer to \
                        in the course of this game.^";
    if (etype==ITGONE_PE)
             {   print "You can't see ~", (address) vague_word,
                       "~ ("; DefArt(vague_obj); print ") at the moment.^"; }
    if (etype==JUNKAFTER_PE)
                 print "I didn't understand the way that finished.^";
    if (etype==TOOFEW_PE)
             {   if (multi_had==0) print "None";
                 else { print "Only "; EnglishNumber(multi_had); }
                 print " of those ";
                 if (multi_had==1) print "is"; else print "are";
                 print " available.^"; }
    if (etype==NOTHING_PE)
             {   if (multi_wanted==100) print "Nothing to do!^";
                 else print "There are none at all available!^";  }
    if (etype==ASKSCOPE_PE)
    {            scope_stage=3;
                 if (indirect(scope_error)==-1)
                 {   best_etype=nextbest_etype; jump GiveError;  }
    }

!  **** (J) ****

!  And go (almost) right back to square one...

    jump ReType;

!  ...being careful not to go all the way back, to avoid infinite repetition
!  of a deferred command causing an error.


!  **** (K) ****

!  At this point, the return value is all prepared, and we are only looking
!  to see if there is a "then" followed by subsequent instruction(s).
    
   .LookForMore;

   if (wn>num_words) rtrue;

   i=NextWord();
   if (i=='then' || i==comma_word)
   {   if (wn>num_words)
       { parse2->1=(parse2->1)-1; held_back_mode = 0; rtrue; }
       if (actor==player) j=0; else j=verb_wordnum-1;
       last_command_from = j+1; last_command_to = wn-2;
       i=NextWord();
       if (i=='again' or #n$g)
       {   for (i=0: i<j: i++)
           {   parse2-->(2*i+1) = parse-->(2*i+1);
               parse2-->(2*i+2) = parse-->(2*i+2);
           }
           for (i=last_command_from:i<=last_command_to:i++, j++)
           {   parse2-->(2+2*j) = parse-->(2*i);
               parse2-->(1+2*j) = parse-->(2*i-1);
           }
           for (i=wn:i<=num_words:i++, j++)
           {   parse2-->(2+2*j) = parse-->(2*i);
               parse2-->(1+2*j) = parse-->(2*i-1);
           }
           parse2->1=j; held_back_mode = 1; rtrue;
       }
       else wn--;
       for (i=0: i<j: i++)
       {   parse2-->(2*i+1) = parse-->(2*i+1);
           parse2-->(2*i+2) = parse-->(2*i+2);
       }
       for (i=wn:i<=num_words:i++, j++)
       {   parse2-->(2+2*j) = parse-->(2*i);
           parse2-->(1+2*j) = parse-->(2*i-1);
       }
       parse2->1=j; held_back_mode = 1; rtrue;
   }
   best_etype=UPTO_PE; jump GiveError;
];

! ----------------------------------------------------------------------------
!  NumberWord - fairly self-explanatory
! ----------------------------------------------------------------------------

[ NumberWord o;
  if (o=='one') return 1;
  if (o=='two') return 2;
  if (o=='three') return 3;
  if (o=='four') return 4;
  if (o=='five') return 5;
  if (o=='six') return 6;
  if (o=='seven') return 7;
  if (o=='eight') return 8;
  if (o=='nine') return 9;
  if (o=='ten') return 10;
  if (o=='eleven') return 11;
  if (o=='twelve') return 12;
  if (o=='thirteen') return 13;
  if (o=='fourteen') return 14;
  if (o=='fifteen') return 15;
  if (o=='sixteen') return 16;
  if (o=='seventeen') return 17;
  if (o=='eighteen') return 18;
  if (o=='nineteen') return 19;
  if (o=='twenty') return 20;
  return 0;
];

! ----------------------------------------------------------------------------
!  Descriptors()
!
!  Handles descriptive words like "my", "his", "another" and so on.
!  Skips "the", and leaves wn pointing to the first misunderstood word.
!
!  Allowed to set up for a plural only if allow_p is set
!
!  Returns error number, or 0 if no error occurred
! ----------------------------------------------------------------------------

Constant OTHER_BIT    1;     !  These will be used in Adjudicate()
Constant MY_BIT       2;     !  to disambiguate choices
Constant THAT_BIT     4;
Constant PLURAL_BIT   8;
Constant ITS_BIT     16;
Constant HIS_BIT     32;
Constant LIT_BIT     64;
Constant UNLIT_BIT  128;

[ Descriptors context  o flag n;

   indef_mode=0; indef_type=0; indef_wanted=0; indef_guess_p=0;

   for (flag=1:flag==1:)
   {   o=NextWord(); flag=0;
       if (o=='the') flag=1;
       if (o==#n$a or 'an' or 'any' || o=='either' or 'anything')
                            { indef_mode=1; flag=1; }
       if (o=='another' or 'other')
                            { indef_mode=1; flag=1;
                              indef_type = indef_type | OTHER_BIT; }
       if (o=='my' or 'this' or 'these')
                            { indef_mode=1; flag=1;
                              indef_type = indef_type | MY_BIT; }
       if (o=='that' or 'those')
                            { indef_mode=1; flag=1;
                              indef_type = indef_type | THAT_BIT; }
       if (o=='its')
                            { indef_mode=1; flag=1;
                              indef_type = indef_type | ITS_BIT; }
       if (o=='his' or 'your')
                            { indef_mode=1; flag=1;
                              indef_type = indef_type | HIS_BIT; }
       if (o=='lit' or 'lighted')
                            { indef_mode=1; flag=1;
                              indef_type = indef_type | LIT_BIT; }
       if (o=='unlit')
                            { indef_mode=1; flag=1;
                              indef_type = indef_type | UNLIT_BIT; }
       if (o=='all' or 'each' or 'every' || o=='everything')
                            { indef_mode=1; flag=1; indef_wanted=100;
                              if (take_all_rule == 1)
                                  take_all_rule = 2;
                              indef_type = indef_type | PLURAL_BIT; }
       if (allow_plurals==1)
       {   n=NumberWord(o);
           if (n>1)         { indef_guess_p=1;
                              indef_mode=1; flag=1; indef_wanted=n;
                              indef_type = indef_type | PLURAL_BIT; }
       }
       if (flag==1 && NextWord() ~= 'of') wn--;  ! Skip 'of' after these
   }
   wn--;
   if ((indef_wanted > 0) && (context<2 || context>5)) return MULTI_PE;
   return 0;
];

! ----------------------------------------------------------------------------
!  CreatureTest: Will this person do for a "creature" token?
! ----------------------------------------------------------------------------

[ CreatureTest obj;
  if (obj has animate) rtrue;
  if (obj hasnt talkable) rfalse;
  if (action_to_be==##Ask or ##Answer or ##Tell
      || action_to_be==##AskFor) rtrue;
  rfalse;
];

! ----------------------------------------------------------------------------
!  ParseObjectList: Parses tokens 0 to 179, from the current word number wn
!
!  Returns:
!    REPARSE_CODE for "reconstructed input, please re-parse from scratch"
!    1            for "token accepted"
!    0            for "token failed"
!
!  (A)            Preliminaries and special/number tokens
!  (B)            Actual object names (mostly subcontracted!)
!  (C)            and/but and so on
!  (D)            Returning an accepted token
!
! ----------------------------------------------------------------------------

[ ParseObjectList results token  l o i j k
                                 and_parity single_object desc_wn many_flag;

    many_flag=0; and_parity=1; dont_infer=0;

!  **** (A) ****
!  We expect to find a list of objects next in what the player's typed.

  .ObjectList;

#ifdef DEBUG;
   if (parser_trace>=3) print "  [Object list from word ", wn, "]^";
#endif;

!  Take an advance look at the next word: if it's "it" or "them", and these
!  are unset, set the appropriate error number and give up on the line
!  (if not, these are still parsed in the usual way - it is not assumed
!  that they still refer to something in scope)

    o=NextWord(); wn--;
    if (o=='it' or 'them')
    {   vague_word=o; vague_obj=itobj;
        if (itobj==0) { etype=VAGUE_PE; return 0; }
    }
    if (o=='him')
    {   vague_word=o; vague_obj=himobj;
        if (himobj==0) { etype=VAGUE_PE; return 0; }
    }
    if (o=='her')
    {   vague_word=o; vague_obj=herobj;
        if (herobj==0) { etype=VAGUE_PE; return 0; }
    }
    if (o=='me' or 'myself' or 'self')
    {   vague_word=o; vague_obj=player;
    }

!  Firstly, get rid of tokens 7 and 8 ("special" and "number"), and
!  tokens which are entirely handed out to outside routines

    if (token==7)
    {   l=TryNumber(wn);
        if (l~=-1000)
        {   if (nsns==0) special_number1=l; else special_number2=l;
            special_number=l;
            nsns++;
#ifdef DEBUG;
            if (parser_trace>=3)
                print "  [Read special as the number ", l, "]^";
#endif;
        }
#ifdef DEBUG;
        if (parser_trace>=3)
            print "  [Read special word at word number ", wn, "]^";
#endif;
        special_word=NextWord(); single_object=1; jump PassToken;
    }
    if (token==8)
    {   l=TryNumber(wn++);
        if (l==-1000) { etype=NUMBER_PE; rfalse; }
#ifdef DEBUG;
        if (parser_trace>=3) print "  [Read number as ", l, "]^";
#endif;
        if (nsns++==0) special_number1=l; else special_number2=l;
        single_object=1; jump PassToken;
    }

    if (token>=48 && token<80)
    {   l=indirect(#preactions_table-->(token-48));
#ifdef DEBUG;
        if (parser_trace>=3)
            print "  [Outside parsing routine returned ", l, "]^";
#endif;
        if (l<0) rfalse;
        if (l==0) { params_wanted--; rtrue; }  ! An adjective after all...
        if (l==1)
        {   if (nsns==0) special_number1=parsed_number;
            else special_number2=parsed_number;
            nsns++;
        }
        if (l==REPARSE_CODE) return l;
        single_object=l; jump PassToken;
    }

    if (token>=80 && token<128)
    {   scope_token = #preactions_table-->(token-80);
        scope_stage = 1;
        l=indirect(scope_token);
#ifdef DEBUG;
        if (parser_trace>=3)
            print "  [Scope routine returned multiple-flag of ", l, "]^";
#endif;
        if (l==1) token=2; else token=0;
    }

    token_was=0;
    if (token>=16)
    {   token_was = token;
        token=0;
    }

!  Otherwise, we have one of the tokens 0 to 6, all of which really do mean
!  that objects are expected.

!  So now we parse any descriptive words

    allow_plurals = 1; desc_wn = wn;
    .TryAgain;

    l=Descriptors(token); if (l~=0) { etype=l; return 0; }

!  **** (B) ****

!  This is an actual specified object, and is therefore where a typing error
!  is most likely to occur, so we set:

    oops_from=wn;

!  In either case below we use NounDomain, giving it the token number as
!  context, and two places to look: among the actor's possessions, and in the
!  present location.  (Note that the order depends on which is likeliest.)

!  So, two cases.  Case 1: token not equal to "held" (so, no implicit takes)
!  but we may well be dealing with multiple objects

    if (token~=1)
    {   i=multiple_object-->0;
#ifdef DEBUG;
        if (parser_trace>=3)
            print "  [Calling NounDomain on location and actor]^";
#endif;
        l=NounDomain(actors_location, actor, token);
        if (l==REPARSE_CODE) return l;                    ! Reparse after Q&A
        if (l==0) { etype=CantSee(); jump FailToken; }  ! Choose best error
#ifdef DEBUG;
        if (parser_trace>=3)
        {   if (l>1)
            {   print "  [ND returned "; DefArt(l); print "]^"; }
            else
            {   print "  [ND appended to the multiple object list:^";
                k=multiple_object-->0;
                for (j=i+1:j<=k:j++)
                {   print "  Entry ", j, ": "; CDefArt(multiple_object-->j);
                    print " (", multiple_object-->j, ")^";
                }
                print "  List now has size ", k, "]^";
            }
        }
#endif;
        if (l==1)
        {   if (many_flag==0)
            {   many_flag=1;
            }
            else                                  ! Merge with earlier ones
            {   k=multiple_object-->0;            ! (with either parity)
                multiple_object-->0 = i;
                for (j=i+1:j<=k:j++)
                {   if (and_parity==1) MultiAdd(multiple_object-->j);
                    else MultiSub(multiple_object-->j);
                }
#ifdef DEBUG;
        if (parser_trace>=3)
            print "  [Merging ", k-i, " new objects to the ", i, " old ones]^";
#endif;
            }
        }
        else
        {   if (token==6 && CreatureTest(l)==0)   ! Animation is required
            {   etype=ANIMA_PE; jump FailToken; } ! for token 6
            if (many_flag==0)
                single_object = l;
            else
            {   if (and_parity==1) MultiAdd(l); else MultiSub(l);
#ifdef DEBUG;
                if (parser_trace>=3)
                {   print "  [Combining "; DefArt(l); print " with list]^";
                }
#endif;
            }
        }
    }

!  Case 2: token is "held" (which fortunately can't take multiple objects)
!  and may generate an implicit take

    if (token==1)
    {   l=NounDomain(actor,actors_location,token);       ! Same as above...
        if (l==REPARSE_CODE) return l;
        if (l==0) { etype=CantSee(); return l; }

!  ...until it produces something not held by the actor.  Then an implicit
!  take must be tried.  If this is already happening anyway, things are too
!  confused and we have to give up (but saving the oops marker so as to get
!  it on the right word afterwards).
!  The point of this last rule is that a sequence like
!
!      > read newspaper
!      (taking the newspaper first)
!      The dwarf unexpectedly prevents you from taking the newspaper!
!
!  should not be allowed to go into an infinite repeat - read becomes
!  take then read, but take has no effect, so read becomes take then read...
!  Anyway for now all we do is record the number of the object to take.

        o=parent(l);
        if (o~=actor)
        {   if (notheld_mode==1)
            {   saved_oops=oops_from; etype=NOTHELD_PE; jump FailToken;
            }
            not_holding = l;
#ifdef DEBUG;
            if (parser_trace>=3)
            {   print "  [Allowing object "; DefArt(l); print " for now]^";
            }
#endif;
        }
        single_object = l;
    }

!  The following moves the word marker to just past the named object...

    wn = oops_from + match_length;

!  **** (C) ****

!  Object(s) specified now: is that the end of the list, or have we reached
!  "and", "but" and so on?  If so, create a multiple-object list if we
!  haven't already (and are allowed to).

    .NextInList;

    o=NextWord();

    if (o=='and' or 'but' or 'except' || o==comma_word)
    {
#ifdef DEBUG;
        if (parser_trace>=3)
        {   print "  [Read '", (address) o, "']^";
        }
#endif;
        if (token<2 || token>=6) { etype=MULTI_PE; jump FailToken; }

        if (o=='but' or 'except') and_parity = 1-and_parity;

        if (many_flag==0)
        {   multiple_object-->0 = 1;
            multiple_object-->1 = single_object;
            many_flag=1;
#ifdef DEBUG;
            if (parser_trace>=3)
            {   print "  [Making new list from ";
                DefArt(single_object); print "]^";
            }
#endif;
        }
        dont_infer = 1; inferfrom=0;              ! Don't print (inferences)
        jump ObjectList;                          ! And back around
    }

    wn--;   ! Word marker back to first not-understood word

!  **** (D) ****

!  Happy or unhappy endings:

    .PassToken;

    if (many_flag==1)
    {   single_object = 0;
        multi_context = token;
    }
    else
    {   if (indef_mode==1 && indef_type & PLURAL_BIT ~= 0)
        {   if (indef_wanted<100 && indef_wanted>1)
            {   multi_had=1; multi_wanted=indef_wanted;
                etype=TOOFEW_PE;
                jump FailToken;
            }
        }
    }
    results-->(parameters+2) = single_object;
    parameters++;
    pattern-->pcount = single_object;
    return 1;

    .FailToken;

!  If we were only guessing about it being a plural, try again but only
!  allowing singulars (so that words like "six" are not swallowed up as
!  Descriptors)

    if (allow_plurals==1 && indef_guess_p==1)
    {   allow_plurals=0; wn=desc_wn; jump TryAgain;
    }
    return 0;
];

! ----------------------------------------------------------------------------
!  NounDomain does the most substantial part of parsing an object name.
!
!  It is given two "domains" - usually a location and then the actor who is
!  looking - and a context (i.e. token type), and returns:
!
!   0    if no match at all could be made,
!   1    if a multiple object was made,
!   k    if object k was the one decided upon,
!   REPARSE_CODE if it asked a question of the player and consequently rewrote all
!        the player's input, so that the whole parser should start again
!        on the rewritten input.
!
!   In the case when it returns 1<k<REPARSE_CODE, it also sets the variable
!   length_of_noun to the number of words in the input text matched to the
!   noun.
!   In the case k=1, the multiple objects are added to multiple_object by
!   hand (not by MultiAdd, because we want to allow duplicates).
! ----------------------------------------------------------------------------

[ NounDomain domain1 domain2 context  first_word i j k l oldw
                                      answer_words marker;

#ifdef DEBUG;
  if (parser_trace>=4) print "   [NounDomain called at word ", wn, "^";
#endif;

  match_length=0; number_matched=0; match_from=wn; placed_in_flag=0;

  SearchScope(domain1, domain2, context);

#ifdef DEBUG;
  if (parser_trace>=4) print "   [ND made ", number_matched, " matches]^";
#endif;

  wn=match_from+match_length;

!  If nothing worked at all, leave with the word marker skipped past the
!  first unmatched word...

  if (number_matched==0) { wn++; rfalse; }

!  Suppose that there really were some words being parsed (i.e., we did
!  not just infer).  If so, and if there was only one match, it must be
!  right and we return it...

  if (match_from <= num_words)
  {   if (number_matched==1) { i=match_list-->0; return i; }

!  ...now suppose that there was more typing to come, i.e. suppose that
!  the user entered something beyond this noun.  Use the lookahead token
!  to check that if an adjective comes next, it is the right one.  (If
!  not then there must be a mistake like "press red buttno" where "red"
!  has been taken for the noun in the mistaken belief that "buttno" is
!  some preposition or other.)
!
!  If nothing ought to follow, then similarly there must be a mistake,
!  (unless what does follow is just a full stop, and or comma)

      if (wn<=num_words)
      {   i=NextWord(); wn--;
          if ((i~='and' or comma_word or 'then')
              && (i~='but' or 'except'))
          {   if (lookahead==8) rfalse;
              if (lookahead>8)
              {   if (lookahead~=AdjectiveWord())
                  { wn--;
#ifdef DEBUG;
                    if (parser_trace>=3)
                    print "   [ND failed at lookahead at word ", wn, "]^";
#endif;
                    rfalse;
                  }
                  wn--;
              }
          }
      }
  }

!  Now look for a good choice, if there's more than one choice...

  number_of_classes=0;
  
  if (number_matched==1) i=match_list-->0;
  if (number_matched>1)
  {   i=Adjudicate(context);
      if (i==-1) rfalse;
      if (i==1) rtrue;       !  Adjudicate has made a multiple
                             !  object, and we pass it on
  }

!  If i is non-zero here, one of two things is happening: either
!  (a) an inference has been successfully made that object i is
!      the intended one from the user's specification, or
!  (b) the user finished typing some time ago, but we've decided
!      on i because it's the only possible choice.
!  In either case we have to keep the pattern up to date,
!  note that an inference has been made and return.
!  (Except, we don't note which of a pile of identical objects.)

  if (i~=0)
  {   if (dont_infer==1) return i;
      if (inferfrom==0) inferfrom=pcount;
      pattern-->pcount = i;
      return i;
  }

!  If we get here, there was no obvious choice of object to make.  If in
!  fact we've already gone past the end of the player's typing (which
!  means the match list must contain every object in scope, regardless
!  of its name), then it's foolish to give an enormous list to choose
!  from - instead we go and ask a more suitable question...

  if (match_from > num_words) jump Incomplete;

!  Now we print up the question, using the equivalence classes as worked
!  out by Adjudicate() so as not to repeat ourselves on plural objects...

  if (context==6) print "Who"; else print "Which";
  print " do you mean, ";
  j=number_of_classes; marker=0;
  for (i=1:i<=number_of_classes:i++)
  {   
      while (((match_classes-->marker) ~= i)
             && ((match_classes-->marker) ~= -i)) marker++;
      k=match_list-->marker;

      if (match_classes-->marker > 0) DefArt(k); else InDefArt(k);

      if (i<j-1)  print ", ";
      if (i==j-1) print " or ";
  }
  print "?^";

!  ...and get an answer:

  .WhichOne;
  answer_words=Keyboard(buffer2, parse2);

  first_word=(parse2-->1);

!  Take care of "all", because that does something too clever here to do
!  later on:

  if ((first_word=='all' or 'both' or 'everything')
      || (first_word=='every' or 'each'))
  {   
      if (context>=2 && context<=5)
      {   l=multiple_object-->0;
          for (i=0:i<number_matched && l+i<63:i++)
          {   k=match_list-->i;
              multiple_object-->(i+1+l) = k;
          }
          multiple_object-->0 = i+l;
          rtrue;
      }
      print "Sorry, you can only have one item here.  Which one exactly?^";
      jump WhichOne;
  }

!  If the first word of the reply can be interpreted as a verb, then
!  assume that the player has ignored the question and given a new
!  command altogether.
!  (This is one time when it's convenient that the directions are
!  not themselves verbs - thus, "north" as a reply to "Which, the north
!  or south door" is not treated as a fresh command but as an answer.)

  j=first_word->#dict_par1;
  if (0~=j&1)
  {   CopyBuffer(buffer, buffer2);
      CopyBuffer(parse, parse2);
      return REPARSE_CODE;
  }

!  Now we insert the answer into the original typed command, as
!  words additionally describing the same object
!  (eg, > take red button
!       Which one, ...
!       > music
!  becomes "take music red button".  The parser will thus have three
!  words to work from next time, not two.)
!
!  To do this we use MoveWord which copies in a word.

  oldw=parse->1;
  parse->1 = answer_words+oldw;

  for (k=oldw+answer_words : k>match_from : k--)
      MoveWord(k, parse, k-answer_words);

  for (k=1:k<=answer_words:k++)
      MoveWord(match_from+k-1, parse2, k);

!  Having reconstructed the input, we warn the parser accordingly
!  and get out.

  return REPARSE_CODE;

!  Now we come to the question asked when the input has run out
!  and can't easily be guessed (eg, the player typed "take" and there
!  were plenty of things which might have been meant).

  .Incomplete;

  if (context==6) print "Whom"; else print "What";
  print " do you want";
  if (actor~=player) { print " "; DefArt(actor); }
  print " to "; PrintCommand(0,1); print "?^";

  answer_words=Keyboard(buffer2, parse2);

  first_word=(parse2-->1);

!  Once again, if the reply looks like a command, give it to the
!  parser to get on with and forget about the question...

  j=first_word->#dict_par1;
  if (0~=j&1)
  {   CopyBuffer(buffer, buffer2);
      CopyBuffer(parse, parse2);
      return REPARSE_CODE;
  }

!  ...but if we have a genuine answer, then we adjoin the words
!  typed onto the expression.  But if we've just inferred something
!  which wasn't actually there, we must adjoin that as well.  (Note
!  the sneaky use of "it" to match an object inferred this time round.)

  oldw=parse->1;
  if (inferfrom==0)
      for (k=1:k<=answer_words:k++)
          MoveWord(oldw+k, parse2, k);
  else
  {   j=pcount-inferfrom;
      for (k=1:k<=answer_words:k++)
          MoveWord(oldw+k+j, parse2, k);
      for (j=inferfrom:j<pcount:j++)
      {   if (pattern-->j >= 2 && pattern-->j < REPARSE_CODE)
          {   parse2-->1 = 'it'; itobj = pattern-->j;
          }
          else parse2-->1 = AdjectiveAddress((pattern-->j) - REPARSE_CODE);
          MoveWord(oldw+1+j-inferfrom, parse2, 1);
          answer_words++;
      }
  }
  parse->1 = answer_words+oldw;

!  And go back to the parser.
  return REPARSE_CODE;
];

! ----------------------------------------------------------------------------
!  The Adjudicate routine tries to see if there is an obvious choice, when
!  faced with a list of objects (the match_list) each of which matches the
!  player's specification equally well.
!
!  To do this it makes use of the context (the token type being worked on).
!  It counts up the number of obvious choices for the given context
!  (all to do with where a candidate is, except for 6 (animate) which is to
!  do with whether it is animate or not);
!
!  if only one obvious choice is found, that is returned;
!
!  if we are in indefinite mode (don't care which) one of the obvious choices
!    is returned, or if there is no obvious choice then an unobvious one is
!    made;
!
!  at this stage, we work out whether the objects are distinguishable from
!    each other or not: if they are all indistinguishable from each other,
!    then choose one, it doesn't matter which;
!
!  otherwise, 0 (meaning, unable to decide) is returned (but remember that
!    the equivalence classes we've just worked out will be needed by other
!    routines to clear up this mess, so we can't economise on working them
!    out).
!
!  Returns -1 if an error occurred
! ----------------------------------------------------------------------------

[ Adjudicate context i j k good_ones last n ultimate flag offset;

#ifdef DEBUG;
  if (parser_trace>=4)
      print "   [Adjudicating match list of size ", number_matched, "^";
#endif;

  j=number_matched-1; good_ones=0; last=match_list-->0;
  for (i=0:i<=j:i++)
  {   n=match_list-->i;
      if (n hasnt concealed)
      {   ultimate=n;
          do
              ultimate=parent(ultimate);
          until (ultimate==actors_location or actor or 0);

          if (context==0 && ultimate==actors_location &&
              (token_was==0 || UserFilter(n)==1)) { good_ones++; last=n; }
          if (context==1 && parent(n)==actor)     { good_ones++; last=n; }
          if (context==2 && ultimate==actors_location) 
                                                  { good_ones++; last=n; }
          if (context==3 && parent(n)==actor)     { good_ones++; last=n; }

          if (context==4 or 5)
          {   if (advance_warning==-1)
              {   if (parent(n)==actor) { good_ones++; last=n; }
              }
              else
              {   if (context==4 && parent(n)==actor && n~=advance_warning)
                  { good_ones++; last=n; }
                  if (context==5 && parent(n)==actor && n in advance_warning)
                  { good_ones++; last=n; }
              }
          }
          if (context==6 && CreatureTest(n)==1)   { good_ones++; last=n; }
      }
  }
  if (good_ones==1) return last;

  ! If there is ambiguity about what was typed, but it definitely wasn't
  ! animate as required, then return anything; higher up in the parser
  ! a suitable error will be given.  (This prevents a question being asked.)
  !
  if (context==6 && good_ones==0) return match_list-->0;

  if (indef_mode==1 && indef_type & PLURAL_BIT ~= 0)
  {   if (context<2 || context>5) { etype=MULTI_PE; return -1; }
      i=0; number_of_classes=1; offset=multiple_object-->0;
      for (j=BestGuess():j~=-1 && i<indef_wanted
           && i+offset<63:j=BestGuess())
      {   flag=0;
          if (j hasnt concealed && j hasnt worn) flag=1;
          if (context==3 or 4 && parent(j)~=actor) flag=0;
          k=ChooseObjects(j,flag);
          if (k==1) flag=1; else { if (k==2) flag=0; }
          if (flag==1)
          {   i++; multiple_object-->(i+offset) = j;
#ifdef DEBUG;
              if (parser_trace>=4) print "   Accepting it^";
#endif;
          }
          else
          {   i=i;
#ifdef DEBUG;
              if (parser_trace>=4) print "   Rejecting it^";
#endif;
          }
      }
      if (i<indef_wanted && indef_wanted<100)
      {   etype=TOOFEW_PE; multi_wanted=indef_wanted;
          multi_had=multiple_object-->0;
          return -1;
      }
      multiple_object-->0 = i+offset;
      multi_context=context;
#ifdef DEBUG;
      if (parser_trace>=4)
          print "   Made multiple object of size ", i, "]^";
#endif;
      return 1;
  }

  for (i=0:i<number_matched:i++) match_classes-->i=0;

  n=1;
  for (i=0:i<number_matched:i++)
      if (match_classes-->i==0)
      {   match_classes-->i=n++; flag=0;
          for (j=i+1:j<number_matched:j++)
              if (match_classes-->j==0
                  && Identical(match_list-->i, match_list-->j)==1)
              {   flag=1;
                  match_classes-->j=match_classes-->i;
              }
          if (flag==1) match_classes-->i = 1-n;
      }
  n--;

#ifdef DEBUG;
  if (parser_trace>=4)
  {   print "   Difficult adjudication with ", n, " equivalence classes:^";
      for (i=0:i<number_matched:i++)
      {   print "   "; CDefArt(match_list-->i);
          print " (", match_list-->i, ")  ---  ",match_classes-->i, "^";
      }
  }
#endif;

  number_of_classes = n;

  if (n>1 && indef_mode==0)
  {   j=0; good_ones=0;
      for (i=0:i<number_matched:i++)
      {   k=ChooseObjects(match_list-->i,2);
          if (k==j) good_ones++;
          if (k>j) { j=k; good_ones=1; last=match_list-->i; }
      }
      if (good_ones==1)
      {
#ifdef DEBUG;
          if (parser_trace>=4)
              print "   ChooseObjects picked a best.]^";
#endif;
          return last;
      }
#ifdef DEBUG;
      if (parser_trace>=4)
          print "   Unable to decide: it's a draw.]^";
#endif;
      return 0;
  }

!  When the player is really vague, or there's a single collection of
!  indistinguishable objects to choose from, choose the one the player
!  most recently acquired, or if the player has none of them, then
!  the one most recently put where it is.

  if (indef_mode==0) indef_type=0;
  if (n==1) dont_infer = 1;

  return BestGuess();
];

! ----------------------------------------------------------------------------
!  ReviseMulti  revises the multiple object which already exists, in the
!    light of information which has come along since then (i.e., the second
!    parameter).  It returns a parser error number, or else 0 if all is well.
!    This only ever throws things out, never adds new ones.
! ----------------------------------------------------------------------------

[ ReviseMulti second_p  i low;

#ifdef DEBUG;
  if (parser_trace>=4)
      print "   Revising multiple object list of size ", multiple_object-->0,
            " with 2nd ", (name) second_p, "^";
#endif;

  if (multi_context==4 or 5)
  {   for (i=1, low=0:i<=multiple_object-->0:i++)
      {   if ( (multi_context==4 && multiple_object-->i ~= second_p)
               || (multi_context==5 && multiple_object-->i in second_p))
          {   low++; multiple_object-->low = multiple_object-->i;
          }
      }
      multiple_object-->0 = low;
  }

  if (multi_context==2)
  {   for (i=1, low=0:i<=multiple_object-->0:i++)
          if (parent(multiple_object-->i)==parent(actor)) low++;
#ifdef DEBUG;
      if (parser_trace>=4)
          print "   Token 2 plural case: number with actor ", low, "^";
#endif;
      if (take_all_rule==2 || low>0)
      {   for (i=1, low=0:i<=multiple_object-->0:i++)
          {   if (parent(multiple_object-->i)==parent(actor))
              {   low++; multiple_object-->low = multiple_object-->i;
              }
          }
          multiple_object-->0 = low;
      }
  }

  i=multiple_object-->0;
#ifdef DEBUG;
  if (parser_trace>=4)
      print "   Done: new size ", i, "^";
#endif;
  if (i==0) return NOTHING_PE;
  return 0;
];

! ----------------------------------------------------------------------------
!  ScoreMatchL  scores the match list for quality in terms of what the
!  player has vaguely asked for.  Points are awarded for conforming with
!  requirements like "my", and so on.  If the score is less than the
!  threshold, block out the entry to -1.
!  The scores are put in the match_classes array, which we can safely
!  reuse by now.
! ----------------------------------------------------------------------------

[ ScoreMatchL  its_owner its_score obj i threshold a_s l_s;

  if (indef_type & OTHER_BIT ~= 0) threshold=40;
  if (indef_type & MY_BIT ~= 0)    threshold=threshold+40;
  if (indef_type & THAT_BIT ~= 0)  threshold=threshold+40;
  if (indef_type & ITS_BIT ~= 0)   threshold=threshold+40;
  if (indef_type & HIS_BIT ~= 0)   threshold=threshold+40;
  if (indef_type & LIT_BIT ~= 0)   threshold=threshold+40;
  if (indef_type & UNLIT_BIT ~= 0) threshold=threshold+40;

#ifdef DEBUG;
  if (parser_trace>=4) print "   Scoring match list with type ", indef_type,
      ", threshold ", threshold, ":^";
#endif;

  a_s = 30; l_s = 20;
  if (action_to_be == ##Take or ##Remove) { a_s=20; l_s=30; }

  for (i=0:i<number_matched:i++)
  {   obj = match_list-->i; its_owner = parent(obj); its_score=0;
      if (its_owner==actor)   its_score=a_s;
      if (its_owner==actors_location) its_score=l_s;
      if (its_score==0 && its_owner~=compass) its_score=10;

      if (indef_type & OTHER_BIT ~=0
          &&  obj~=itobj or himobj or herobj)
          its_score=its_score+40;
      if (indef_type & MY_BIT ~=0  &&  its_owner==actor)
          its_score=its_score+40;
      if (indef_type & THAT_BIT ~=0  &&  its_owner==actors_location)
          its_score=its_score+40;
      if (indef_type & LIT_BIT ~=0  &&  obj has light)
          its_score=its_score+40;
      if (indef_type & UNLIT_BIT ~=0  &&  obj hasnt light)
          its_score=its_score+40;
      if (indef_type & ITS_BIT ~=0  &&  its_owner==itobj)
          its_score=its_score+40;
      if (indef_type & HIS_BIT ~=0  &&  its_owner has animate
          && GetGender(its_owner)==1)
          its_score=its_score+40;

      its_score=its_score + ChooseObjects(obj,2);

      if (its_score < threshold) match_list-->i=-1;
      else
      {   match_classes-->i=its_score;
#ifdef DEBUG;
          if (parser_trace >= 4)
          {   print "   "; CDefArt(match_list-->i);
              print " (", match_list-->i, ") in "; DefArt(its_owner);
              print " scores ",its_score, "^";
          }
#endif;
      }
  }
  number_of_classes=2;
];

! ----------------------------------------------------------------------------
!  BestGuess makes the best guess it can out of the match list, assuming that
!  everything in the match list is textually as good as everything else;
!  however it ignores items marked as -1, and so marks anything it chooses.
!  It returns -1 if there are no possible choices.
! ----------------------------------------------------------------------------

[ BestGuess  earliest its_score best i;

  if (number_of_classes~=1) ScoreMatchL();

  earliest=0; best=-1;
  for (i=0:i<number_matched:i++)
  {   if (match_list-->i >= 0)
      {   its_score=match_classes-->i;
          if (its_score>best) { best=its_score; earliest=i; }
      }
  }
#ifdef DEBUG;
  if (parser_trace>=4)
  {   if (best<0)
          print "   Best guess ran out of choices^";
      else
      {   print "   Best guess "; DefArt(match_list-->earliest);
          print  " (", match_list-->earliest, ")^";
      }
  }
#endif;
  if (best<0) return -1;
  i=match_list-->earliest;
  match_list-->earliest=-1;
  return i;
];

! ----------------------------------------------------------------------------
!  Identical decides whether or not two objects can be distinguished from
!  each other by anything the player can type.  If not, it returns true.
! ----------------------------------------------------------------------------

[ Identical o1 o2 p1 p2 n1 n2 i j flag;

!  print "Id on ", o1, " (", object o1, ") and ", o2, " (", object o2, ")^";

  if (o1==o2) rtrue;  ! This should never happen, but to be on the safe side
  if (o1==0 || o2==0) rfalse;  ! Similarly
  if (parent(o1)==compass || parent(o2)==compass) rfalse; ! Saves time

!  What complicates things is that o1 or o2 might have a parsing routine,
!  so the parser can't know from here whether they are or aren't the same.
!  If they have different parsing routines, we simply assume they're
!  different.  If they have the same routine (which they probably got from
!  a class definition) then the decision process is as follows:
!
!     the routine is called (with self being o1, not that it matters)
!       with noun and second being set to o1 and o2, and action being set
!       to the fake action TheSame.  If it returns -1, they are found
!       identical; if -2, different; and if >=0, then the usual method
!       is used instead.

  if (o1.parse_name~=0 || o2.parse_name~=0)
  {   if (o1.parse_name ~= o2.parse_name) rfalse;
      parser_action=##TheSame; parser_one=o1; parser_two=o2;
      j=wn; i=RunRoutines(o1,parse_name); wn=j;
      if (i==-1) rtrue; if (i==-2) rfalse;
  }

!  This is the default algorithm: do they have the same words in their
!  "name" (i.e. property no. 1) properties.  (Note that the following allows
!  for repeated words and words in different orders.)

  p1 = o1.&1; n1 = (o1.#1)/2;
  p2 = o2.&1; n2 = (o2.#1)/2;

!  for (i=0:i<n1:i++) { print (address) p1-->i, " "; } new_line;
!  for (i=0:i<n2:i++) { print (address) p2-->i, " "; } new_line;

  for (i=0:i<n1:i++)
  {   flag=0;
      for (j=0:j<n2:j++)
          if (p1-->i == p2-->j) flag=1;
      if (flag==0) rfalse;
  }

  for (j=0:j<n2:j++)
  {   flag=0;
      for (i=0:i<n1:i++)
          if (p1-->i == p2-->j) flag=1;
      if (flag==0) rfalse;
  }

!  print "Which are identical!^";
  rtrue;
];

! ----------------------------------------------------------------------------
!  PrintCommand reconstructs the command as it presently reads, from
!  the pattern which has been built up
!
!  If from is 0, it starts with the verb: then it goes through the pattern.
!  The other parameter is "emptyf" - a flag: if 0, it goes up to pcount:
!  if 1, it goes up to pcount-1.
!
!  Note that verbs and prepositions are printed out of the dictionary:
!  and that since the dictionary may only preserve the first six characters
!  of a word (in a V3 game), we have to hand-code the longer words needed.
!
!  (Recall that pattern entries are 0 for "multiple object", 1 for "special
!  word", 2 to 999 are object numbers and REPARSE_CODE+n means the preposition n)
! ----------------------------------------------------------------------------

[ PrintCommand from emptyf i j k f;
  if (from==0)
  {   i=verb_word; from=1; f=1;
#IFV3;
      if (i=='inventory') { print "take an inventory"; jump VerbPrinted; }
      if (i=='examine')   { print "examine";           jump VerbPrinted; }
      if (i=='discard')   { print "discard";           jump VerbPrinted; }
      if (i=='swallow')   { print "swallow";           jump VerbPrinted; }
      if (i=='embrace')   { print "embrace";           jump VerbPrinted; }
      if (i=='squeeze')   { print "squeeze";           jump VerbPrinted; }
      if (i=='purchase')  { print "purchase";          jump VerbPrinted; }
      if (i=='unscrew')   { print "unscrew";           jump VerbPrinted; }
      if (i=='describe')  { print "describe";          jump VerbPrinted; }
      if (i=='uncover')   { print "uncover";           jump VerbPrinted; }
      if (i=='discard')   { print "discard";           jump VerbPrinted; }
      if (i=='transfer')  { print "transfer";          jump VerbPrinted; }
#ENDIF;
      if (i==#n$l)         { print "look";              jump VerbPrinted; }
      if (i==#n$z)         { print "wait";              jump VerbPrinted; }
      if (i==#n$x)         { print "examine";           jump VerbPrinted; }
      if (i==#n$i or 'inv') { print "inventory";        jump VerbPrinted; }
      if (PrintVerb(i)==0) print (address) i;
  }
  .VerbPrinted;
  j=pcount-emptyf;
  for (k=from:k<=j:k++)
  {   if (f==1) print (char) ' ';
      i=pattern-->k;
      if (i==0) { print "those things"; jump TokenPrinted; }
      if (i==1) { print "that"; jump TokenPrinted; }
      if (i>=REPARSE_CODE)
      {   i=AdjectiveAddress(i-REPARSE_CODE);
#IFV3;
          if (i=='against') { print "against";      jump TokenPrinted; }
#ENDIF;
          print (address) i;
      }
      else DefArt(i);
      .TokenPrinted;
      f=1;
  }
];

! ----------------------------------------------------------------------------
!  The CantSee routine returns a good error number for the situation where
!  the last word looked at didn't seem to refer to any object in context.
!
!  The idea is that: if the actor is in a location (but not inside something
!  like, for instance, a tank which is in that location) then an attempt to
!  refer to one of the words listed as meaningful-but-irrelevant there
!  will cause "you don't need to refer to that in this game" rather than
!  "no such thing" or "what's 'it'?".
!  (The advantage of not having looked at "irrelevant" local nouns until now
!  is that it stops them from clogging up the ambiguity-resolving process.
!  Thus game objects always triumph over scenery.)
! ----------------------------------------------------------------------------

[ CantSee  i w e;
    saved_oops=oops_from;

    if (scope_token~=0) { scope_error = scope_token; return ASKSCOPE_PE; }

    wn--; w=NextWord();
    e=CANTSEE_PE;
    if (w==vague_word) e=ITGONE_PE;
    i=parent(actor);
    if (i has visited && Refers(i,w)==1) e=SCENERY_PE;
    if (etype>e) return etype;
    return e;
];

! ----------------------------------------------------------------------------
!  The MultiAdd routine adds object "o" to the multiple-object-list.
!
!  This is only allowed to hold 63 objects at most, at which point it ignores
!  any new entries (and sets a global flag so that a warning may later be
!  printed if need be).
! ----------------------------------------------------------------------------

[ MultiAdd o i j;
  i=multiple_object-->0;
  if (i==63) { toomany_flag=1; rtrue; }
  for (j=1:j<=i:j++)
      if (o==multiple_object-->j) 
          rtrue;
  i++;
  multiple_object-->i = o;
  multiple_object-->0 = i;
];

! ----------------------------------------------------------------------------
!  The MultiSub routine deletes object "o" from the multiple-object-list.
!
!  It returns 0 if the object was there in the first place, and 9 (because
!  this is the appropriate error number in Parser()) if it wasn't.
! ----------------------------------------------------------------------------

[ MultiSub o i j k et;
  i=multiple_object-->0; et=0;
  for (j=1:j<=i:j++)
      if (o==multiple_object-->j)
      {   for (k=j:k<=i:k++)
              multiple_object-->k = multiple_object-->(k+1);
          multiple_object-->0 = --i;
          return et;
      }
  et=9; return et;
];

! ----------------------------------------------------------------------------
!  The MultiFilter routine goes through the multiple-object-list and throws
!  out anything without the given attribute "attr" set.
! ----------------------------------------------------------------------------

[ MultiFilter attr  i j o;
  .MFiltl;
  i=multiple_object-->0;
  for (j=1:j<=i:j++)
  {   o=multiple_object-->j;
      if (o hasnt attr) { MultiSub(o); jump Mfiltl; }
  }
];

! ----------------------------------------------------------------------------
!  The UserFilter routine consults the user's filter (or checks on attribute)
!  to see what already-accepted nouns are acceptable
! ----------------------------------------------------------------------------

[ UserFilter obj;

  if (token_was>=128)
  {   if (obj has (token_was-128)) rtrue;
      rfalse;
  }
  noun=obj;
  return (indirect(#preactions_table-->(token_was-16)));
];

! ----------------------------------------------------------------------------
!  MoveWord copies word at2 from parse buffer b2 to word at1 in "parse"
!  (the main parse buffer)
! ----------------------------------------------------------------------------

[ MoveWord at1 b2 at2 x y;
  x=at1*2-1; y=at2*2-1;
  parse-->x++ = b2-->y++;
  parse-->x = b2-->y;
];

! ----------------------------------------------------------------------------
!  SearchScope  domain1 domain2 context
!
!  Works out what objects are in scope (possibly asking an outside routine),
!  but does not look at anything the player has typed.
! ----------------------------------------------------------------------------

[ SearchScope domain1 domain2 context i;

  i=0;
!  Everything is in scope to the debugging commands

#ifdef DEBUG;
  if (scope_reason==PARSING_REASON
      && (verb_word == 'purloin' or 'tree' or 'abstract'
          || verb_word == 'gonear' or 'scope'))
  {   for (i=selfobj+1:i<=top_object:i++) PlaceInScope(i);
      rtrue;
  }
#endif;

!  First, a scope token gets priority here:

  if (scope_token ~= 0)
  {   scope_stage=2;
      if (indirect(scope_token)~=0) rtrue;
  }

!  Next, call any user-supplied routine adding things to the scope,
!  which may circumvent the usual routines altogether if they return true:

  if (actor==domain1 or domain2 && InScope(actor)~=0) rtrue;

!  Pick up everything in the location except the actor's possessions;
!  then go through those.  (This ensures the actor's possessions are in
!  scope even in Darkness.)

  if (context==5 && advance_warning ~= -1)
  {   if (IsSeeThrough(advance_warning)==1)
          ScopeWithin(advance_warning, 0, context);
  }
  else
  {   ScopeWithin(domain1, domain2, context);
      ScopeWithin(domain2,0,context);
  }
];

! ----------------------------------------------------------------------------
!  IsSeeThrough is used at various places: roughly speaking, it determines
!  whether o being in scope means that the contents of o are in scope.
! ----------------------------------------------------------------------------

[ IsSeeThrough o;
  if (o has supporter
      || (o has transparent)
      || (o has container && o has open))
      rtrue;
  rfalse;
];

! ----------------------------------------------------------------------------
!  PlaceInScope is provided for routines outside the library, and is not
!  called within the parser (except for debugging purposes).
! ----------------------------------------------------------------------------

[ PlaceInScope thing;
   if (scope_reason~=PARSING_REASON or TALKING_REASON)
   {   DoScopeAction(thing); rtrue; }
   wn=match_from; TryGivenObject(thing); placed_in_flag=1;
];

! ----------------------------------------------------------------------------
!  DoScopeAction
! ----------------------------------------------------------------------------

[ DoScopeAction thing s p1;
  s = scope_reason; p1=parser_one;
#ifdef DEBUG;
  if (parser_trace>=5)
  {   print "[DSA on ", (the) thing, " with reason = ", scope_reason,
      " p1 = ", parser_one, " p2 = ", parser_two, "]^";
  }
#endif;
  switch(scope_reason)
  {   REACT_BEFORE_REASON:
          if (thing.react_before==0 or NULL) return;
#ifdef DEBUG;
          if (parser_trace>=2)
          {   print "[Considering react_before for ", (the) thing, "]^"; }
#endif;
          if (parser_one==0) parser_one = RunRoutines(thing,react_before);
      REACT_AFTER_REASON:
          if (thing.react_after==0 or NULL) return;
#ifdef DEBUG;
          if (parser_trace>=2)
          {   print "[Considering react_after for ", (the) thing, "]^"; }
#endif;
          if (parser_one==0) parser_one = RunRoutines(thing,react_after);
      EACH_TURN_REASON:
          if (thing.&each_turn==0) return;
#ifdef DEBUG;
          if (parser_trace>=2)
          {   print "[Considering each_turn for ", (the) thing, "]^"; }
#endif;
          PrintOrRun(thing, each_turn);
      TESTSCOPE_REASON:
          if (thing==parser_one) parser_two = 1;
      LOOPOVERSCOPE_REASON:
          indirect(parser_one,thing); parser_one=p1;
  }
  scope_reason = s;
];

! ----------------------------------------------------------------------------
!  ScopeWithin looks for objects in the domain which make textual sense
!  and puts them in the match list.  (However, it does not recurse through
!  the second argument.)
! ----------------------------------------------------------------------------

[ ScopeWithin domain nosearch context;

   if (domain==0) rtrue;

!  multiexcept doesn't have second parameter in scope
   if (context==4 && domain==advance_warning) rtrue;

!  Special rule: the directions (interpreted as the 12 walls of a room) are
!  always in context.  (So, e.g., "examine north wall" is always legal.)
!  (Unless we're parsing something like "all", because it would just slow
!  things down then, or unless the context is "creature".)

   if (indef_mode==0 && domain==actors_location
       && scope_reason==PARSING_REASON && context~=6) ScopeWithin(compass);

!  Look through the objects in the domain

   objectloop (domain in domain) ScopeWithin_O(domain, nosearch, context);
];

[ ScopeWithin_O domain nosearch context i ad n;

!  If the scope reason is unusual, don't parse.

      if (scope_reason~=PARSING_REASON or TALKING_REASON)
      {   DoScopeAction(domain); jump DontAccept; }

!  If we're beyond the end of the user's typing, accept everything
!  (NounDomain will sort things out)

      if (match_from > num_words)
      {   i=parser_trace; parser_trace=0;
#ifdef DEBUG;
          if (i>=5) { print "     Out of text: matching "; DefArt(domain);
                      new_line; }
#endif;
          MakeMatch(domain,1);
          parser_trace=i; jump DontAccept;
      }

!  "it" or "them" matches to the it-object only.  (Note that (1) this means
!  that "it" will only be understood if the object in question is still
!  in context, and (2) only one match can ever be made in this case.)

      wn=match_from;
      i=NounWord();
      if (i==1 && itobj==domain)   MakeMatch(itobj,1);
      if (i==2 && himobj==domain)  MakeMatch(himobj,1);
      if (i==3 && herobj==domain)  MakeMatch(herobj,1);
      if (i==4 && player==domain)  MakeMatch(player,1);

!  Construing the current word as the start of a noun, can it refer to the
!  object?

      wn--; TryGivenObject(domain);

      .DontAccept;

!  Shall we consider the possessions of the current object, as well?
!  Only if it's a container (so, for instance, if a dwarf carries a
!  sword, then "drop sword" will not be accepted, but "dwarf, drop sword"
!  will).
!  Also, only if there are such possessions.
!
!  Notice that the parser can see "into" anything flagged as
!  transparent - such as a dwarf whose sword you can get at.

      if (child(domain)~=0 && domain ~= nosearch && IsSeeThrough(domain)==1)
          ScopeWithin(domain,0,context);

!  Drag any extras into context

   ad = domain.&add_to_scope;
   if (ad ~= 0)
   {   if (UnsignedCompare(ad-->0,top_object) > 0)
       {   ats_flag = 2+context;
           RunRoutines(domain, add_to_scope);
           ats_flag = 0;
       }
       else
       {   n=domain.#add_to_scope;
           for (i=0:(2*i)<n:i++)
               ScopeWithin_O(ad-->i,0,context);
       }
   }
];

[ AddToScope obj;
   if (ats_flag>=2)
       ScopeWithin_O(obj,0,ats_flag-2);
   if (ats_flag==1)
   {   if  (HasLightSource(obj)==1) ats_hls = 1;
   }
];

! ----------------------------------------------------------------------------
!  MakeMatch looks at how good a match is.  If it's the best so far, then
!  wipe out all the previous matches and start a new list with this one.
!  If it's only as good as the best so far, add it to the list.
!  If it's worse, ignore it altogether.
!
!  The idea is that "red panic button" is better than "red button" or "panic".
!
!  number_matched (the number of words matched) is set to the current level
!  of quality.
!
!  We never match anything twice, and keep at most 64 equally good items.
! ----------------------------------------------------------------------------

[ MakeMatch obj quality i;
#ifdef DEBUG;
   if (parser_trace>=5) print "    Match with quality ",quality,"^";
#endif;
   if (token_was~=0 && UserFilter(obj)==0)
   {   #ifdef DEBUG;
       if (parser_trace>=5) print "    Match filtered out^";
       #endif;
       rtrue;
   }
   if (quality < match_length) rtrue;
   if (quality > match_length) { match_length=quality; number_matched=0; }
   else
   {   if (2*number_matched>=MATCH_LIST_SIZE) rtrue;
       for (i=0:i<number_matched:i++)
           if (match_list-->i==obj) rtrue;
   }
   match_list-->number_matched++ = obj;
#ifdef DEBUG;
   if (parser_trace>=5) print "    Match added to list^";
#endif;
];

! ----------------------------------------------------------------------------
!  TryGivenObject tries to match as many words as possible in what has been
!  typed to the given object, obj.  If it manages any words matched at all,
!  it calls MakeMatch to say so.  There is no return value.
! ----------------------------------------------------------------------------

[ TryGivenObject obj threshold k w j;

#ifdef DEBUG;
   if (parser_trace>=5)
   {   print "    Trying "; DefArt(obj);
       print " (", obj, ") at word ", wn, "^";
   }
#endif;

!  If input has run out and we're in indefinite mode, then always match,
!  with only quality 0 (this saves time).

   if (indef_mode ~=0 && wn > parse->1) { MakeMatch(obj,0); rfalse; }

!  Ask the object to parse itself if necessary, sitting up and taking notice
!  if it says the plural was used:

   if (obj.parse_name~=0)
   {   parser_action=-1; j=wn;
       k=RunRoutines(obj,parse_name);
       if (k>0)
       {   wn=j+k;
           .MMbyPN;
           if (parser_action == ##PluralFound)
           {   if (allow_plurals == 0) jump NoWordsMatch;
               if (indef_mode==0)
               {   indef_mode=1; indef_type=0; indef_wanted=0; }
               indef_type=indef_type | PLURAL_BIT;
               if (indef_wanted==0) indef_wanted=100;
           }
           MakeMatch(obj,k); rfalse;
       }
       if (k==0) jump NoWordsMatch;
   }

!  The default algorithm is simply to count up how many words pass the
!  Refers test:

   w = NounWord();
   if ((w==1 && obj==itobj)
       || (w==2 && obj==himobj)
       || (w==3 && obj==herobj)
       || (w==4 && obj==player)) { MakeMatch(obj,1); rfalse; }
  
   j=--wn;
   threshold = ParseNoun(obj);
#ifdef DEBUG;
   if (threshold>=0 && parser_trace>=5)
       print "    ParseNoun returned ", threshold, "^";
#endif;
   if (threshold<0) wn++;
   if (threshold>0) { k=threshold; jump MMbyPN; }

   if (threshold==0 || Refers(obj,w)==0)
   {   .NoWordsMatch;
       if (indef_mode~=0) MakeMatch(obj,0);
       rfalse;
   }

   if (threshold<0)
   {   threshold=1; while (0~=Refers(obj,NextWord())) threshold++;
   }

   MakeMatch(obj,threshold);

#ifdef DEBUG;
   if (parser_trace>=5) print "    Matched^";
#endif;
];

! ----------------------------------------------------------------------------
!  Refers works out whether the word with dictionary address wd can refer to
!  the object obj, by seeing if wd is listed in the "names" property of obj.
! ----------------------------------------------------------------------------

[ Refers obj wd   k l m;
    if (obj==0) rfalse;
    k=obj.&1; l=(obj.#1)/2-1;
    for (m=0:m<=l:m++)
        if (wd==k-->m) rtrue;
    rfalse;
];

! ----------------------------------------------------------------------------
!  NounWord (which takes no arguments) returns:
!
!   1  if the next word is "it" or "them",
!   2  if the next word is "him",
!   3  if the next word is "her",
!   4  if "me", "myself", "self"
!   0  if the next word is unrecognised or does not carry the "noun" bit in
!      its dictionary entry,
!   or the address in the dictionary if it is a recognised noun.
!
!  The "current word" marker moves on one.
! ----------------------------------------------------------------------------

[ NounWord i;
   i=NextWord();
   if (i=='it' or 'them') return 1;
   if (i=='him') return 2;
   if (i=='her') return 3;
   if (i=='me' or 'myself' or 'self') return 4;
   if (i==0) rfalse;
   if ((i->#dict_par1)&128 == 0) rfalse;
   return i;
];

! ----------------------------------------------------------------------------
!  AdjectiveWord (which takes no arguments) returns:
!
!   0  if the next word is listed in the dictionary as possibly an adjective,
!   or its adjective number if it is.
!
!  The "current word" marker moves on one.
! ----------------------------------------------------------------------------

[ AdjectiveWord i j;
   j=NextWord();
   if (j==0) rfalse;
   i=j->#dict_par1;
   if (i&8 == 0) rfalse;
   return(j->#dict_par3);
];

! ----------------------------------------------------------------------------
!  AdjectiveAddress works out the address in the dictionary of the word
!  corresponding to the given adjective number.
!
!  It should never produce the given error (which would mean that Inform
!  had set up the adjectives table incorrectly).
! ----------------------------------------------------------------------------

[ AdjectiveAddress n m;
   m=#adjectives_table;
   for (::)
   {   if (n==m-->1) return m-->0;
       m=m+4;
   }
   m=#adjectives_table; RunTimeError(1);
   return m;
];

! ----------------------------------------------------------------------------
!  NextWord (which takes no arguments) returns:
!
!  0            if the next word is unrecognised,
!  comma_word   if it is a comma character
!               (which is treated oddly by the Z-machine, hence the code)
!  or the dictionary address if it is recognised.
!  The "current word" marker is moved on.
!
!  NextWordStopped does the same, but returns -1 when input has run out
! ----------------------------------------------------------------------------

[ NextWord i j k;
   if (wn > parse->1) { wn++; rfalse; }
   i=wn*2-1; wn++;
   j=parse-->i;
   if (j==0)
   {   k=wn*4-3; i=buffer->(parse->k);
       if (i==',') j=comma_word;
       if (i=='.') j='then';
   }
   return j;
];   

[ NextWordStopped;
   if (wn > parse->1) { wn++; return -1; }
   return NextWord();
];

[ WordAddress wordnum;
   return buffer + parse->(wordnum*4+1);
];

[ WordLength wordnum;
   return parse->(wordnum*4);
];

! ----------------------------------------------------------------------------
!  TryNumber is the only routine which really does any character-level
!  parsing, since that's normally left to the Z-machine.
!  It takes word number "wordnum" and tries to parse it as an (unsigned)
!  decimal number, returning
!
!  -1000                if it is not a number
!  the number           if it has between 1 and 4 digits
!  10000                if it has 5 or more digits.
!
!  (The danger of allowing 5 digits is that Z-machine integers are only
!  16 bits long, and anyway this isn't meant to be perfect.)
!
!  Using NumberWord, it also catches "one" up to "twenty".
!
!  Note that a game can provide a ParseNumber routine which takes priority,
!  to enable parsing of odder numbers ("x45y12", say).
! ----------------------------------------------------------------------------

[ TryNumber wordnum   i j c num len mul tot d digit;

   i=wn; wn=wordnum; j=NextWord(); wn=i;
   j=NumberWord(j); if (j>=1) return j;

   i=wordnum*4+1; j=parse->i; num=j+buffer; len=parse->(i-1);

   tot=ParseNumber(num, len);  if (tot~=0) return tot;

   if (len>=4) mul=1000;
   if (len==3) mul=100;
   if (len==2) mul=10;
   if (len==1) mul=1;

   tot=0; c=0; len=len-1;

   for (c=0:c<=len:c++)
   {   digit=num->c;
       if (digit=='0') { d=0; jump digok; }
       if (digit=='1') { d=1; jump digok; }
       if (digit=='2') { d=2; jump digok; }
       if (digit=='3') { d=3; jump digok; }
       if (digit=='4') { d=4; jump digok; }
       if (digit=='5') { d=5; jump digok; }
       if (digit=='6') { d=6; jump digok; }
       if (digit=='7') { d=7; jump digok; }
       if (digit=='8') { d=8; jump digok; }
       if (digit=='9') { d=9; jump digok; }
       return -1000;
     .digok;
       tot=tot+mul*d; mul=mul/10;
   }
   if (len>3) tot=10000;
   return tot;
];

! ----------------------------------------------------------------------------
!  ResetVagueWords does, assuming that i was the object last referred to
! ----------------------------------------------------------------------------

[ ResetVagueWords i;
   if (i has animate && i~=player)
   {   if (GetGender(i)==1) himobj=i;
       else herobj=i;
   }
   else itobj=i;
];

! ----------------------------------------------------------------------------
!  GetGender returns 0 if the given animate object is female, and 1 if male
!  (not all games will want such a simple decision function!)
! ----------------------------------------------------------------------------

[ GetGender person;
   if (person hasnt female) rtrue;
   rfalse;
];

! ----------------------------------------------------------------------------
!  For copying buffers
! ----------------------------------------------------------------------------

[ CopyBuffer bto bfrom i size;
   size=bto->0;
   for (i=1:i<=size:i++) bto->i=bfrom->i;
];

! ----------------------------------------------------------------------------
!  Useful routine: unsigned comparison (for addresses in Z-machine)
!    Returns 1 if x>y, 0 if x=y, -1 if x<y
!  ZRegion(addr) returns 1 if object num, 2 if in code area, 3 if in strings
! ----------------------------------------------------------------------------

[ UnsignedCompare x y u v;
  if (x==y) return 0;
  if (x<0 && y>=0) return 1;
  if (x>=0 && y<0) return -1;
  u = x&$7fff; v= y&$7fff;
  if (u>v) return 1;
  return -1;
];

[ ZRegion addr;
  if (addr==0) return 0;
  if (addr>=1 && addr<=top_object) return 1;
  if (UnsignedCompare(addr, #strings_offset)>=0) return 3;
  if (UnsignedCompare(addr, #code_offset)>=0) return 2;
  return 0;
];

[ PrintOrRun obj prop flag a;
  if (obj.#prop > 2) return RunRoutines(obj,prop);
  if (obj.prop==NULL) rfalse;
  a=ZRegion(obj.prop);
  if (a==0 or 1) return RunTimeError(2,obj,prop);
  if (a==3) { print (string) obj.prop; if (flag==0) new_line; rtrue; }
  return RunRoutines(obj,prop);
];

[ ValueOrRun obj prop a;
  a=ZRegion(obj.prop);
  if (a==2) return RunRoutines(obj,prop);
  return obj.prop;
];

#IFDEF DEBUG;
[ NameTheProperty prop;
  if (#identifiers_table-->prop == 0)
  {   print "property ", prop; return;
  }
  print (string) #identifiers_table-->prop;
];
#ENDIF;

[ RunRoutines obj prop i j k l m ssv;

   if (obj==thedark && prop~=initial) obj=real_location;
   if (obj.prop==NULL or 0) rfalse;

#IFDEF DEBUG;
   if (debug_flag & 1 ~= 0 && prop~=short_name)
   {   print "[Running ", (NameTheProperty) prop,
          " for ", (name) obj,"]^";
   }
#ENDIF;

   j=obj.&prop; k=obj.#prop; m=self; self=obj;
   ssv=sw__var;
   if (prop==life) sw__var=reason_code;
   else sw__var=action;
!   if (prop~=life or orders)
!   {   noun=inp1; second=inp2;
!   }
   for (i=0:i<k/2:i++)
   {   if (j-->i == NULL) { self=m; sw__var=ssv; rfalse; }
       l=ZRegion(j-->i);
       if (l==2)
       {   l=indirect(j-->i);
           if (l~=0) { self=m; sw__var=ssv; return l; }
       }
       else
       {   if (l==3) { print (address) j-->i; new_line; }
           else RunTimeError(3,obj,prop);
       }
   }
   self=m; sw__var=ssv;
   rfalse;
];

! ----------------------------------------------------------------------------
!  End of the parser proper: the remaining routines are its front end.
! ----------------------------------------------------------------------------

[ DisplayStatus;
   if (the_time==NULL)
   {   sline1=score; sline2=turns; }
   else
   {   sline1=the_time/60; sline2=the_time%60; }
];

[ SetTime t s;
   the_time=t; time_rate=s; time_step=0;
   if (s<0) time_step=0-s;
];

[ NotifyTheScore i;
  print "^[Your score has just gone ";
  if (last_score > score) { i=last_score-score; print "down"; }
  else { i=score-last_score; print "up"; }
  print " by "; EnglishNumber(i); print " point";
  if (i>1) print "s"; print ".]^";
];

[ PlayTheGame i j k l;

   standard_interpreter = $32-->0;

   player = selfobj;
   top_object = #largest_object-255;
   selfobj.capacity = MAX_CARRIED;

   self = GameController; sender = GameController;

   j=Initialise();

   last_score = score;
   move player to location;
   while (parent(location)~=0) location=parent(location);
   objectloop (i in player) give i moved ~concealed;

   if (j~=2) Banner();

   lightflag=OffersLight(parent(player));
   if (lightflag==0) { real_location=location; location=thedark; }
   <Look>;

   for (i=1:i<=100:i++) j=random(i);

   while (deadflag==0)
   {   self = GameController; sender = GameController;
       if (score ~= last_score)
       {   if (notify_mode==1) NotifyTheScore(); last_score=score; }

      .late__error;

       inputobjs-->0 = 0; inputobjs-->1 = 0;
       inputobjs-->2 = 0; inputobjs-->3 = 0; meta=0;

       !  The Parser writes its results into inputobjs and meta,
       !  a flag indicating a "meta-verb".  This can only be set for
       !  commands by the player, not for orders to others.

       Parser(inputobjs);

       noun=0; second=0; action=0; multiflag=0;
       onotheld_mode=notheld_mode; notheld_mode=0;

       !  The notheld_mode variables are for implicit taking.

       action=inputobjs-->0;

       !  Reverse "give fred biscuit" into "give biscuit to fred"

       if (action==##GiveR or ##ShowR)
       {   i=inputobjs-->2; inputobjs-->2=inputobjs-->3; inputobjs-->3=i;
           if (action==##GiveR) action=##Give; else action=##Show;
       }

       !  Convert "P, tell me about X" to "ask P about X"

       if (action==##Tell && inputobjs-->2==player && actor~=player)
       {   inputobjs-->2=actor; actor=player; action=##Ask;
       }

       !  Convert "ask P for X" to "P, give X to me"

       if (action==##AskFor && inputobjs-->2~=player && actor==player)
       {   actor=inputobjs-->2; inputobjs-->2=inputobjs-->3;
           inputobjs-->3=player; action=##Give;
       }

       !  For old, obsolete code: special_word contains the topic word
       !  in conversation

       if (action==##Ask or ##Tell or ##Answer)
           special_word = special_number1;

      .begin__action;
       inp1=0; inp2=0; i=inputobjs-->1;
       if (i>=1) inp1=inputobjs-->2;
       if (i>=2) inp2=inputobjs-->3;

       !  inp1 and inp2 hold: object numbers, or 0 for "multiple object",
       !  or 1 for "a number or dictionary address"

       noun=inp1; second=inp2;
       if (inp1==1) noun=special_number1;
       if (inp2==1)
       {   if (inp1==1) second=special_number2;
           else second=special_number1;
       }

       !  noun and second equal inp1 and inp2, except that in place of 1
       !  they substitute the actual number or dictionary address

       if (actor~=player)
       {   
           !  The player's "orders" property can refuse to allow conversation
           !  here, by returning true.  If not, the order is sent to the
           !  other person's "orders" property.  If that also returns false,
           !  then: if it was a misunderstood command anyway, it is converted
           !  to an Answer action (thus "floyd, grrr" ends up as
           !  "say grrr to floyd").  If it was a good command, it is finally
           !  offered to the Order: part of the other person's "life"
           !  property, the old-fashioned way of dealing with conversation.

           j=RunRoutines(player,orders);
           if (j==0)
           {   j=RunRoutines(actor,orders);
               if (j==0)
               {   if (action==##NotUnderstood)
                   {   inputobjs-->3=actor; actor=player; action=##Answer;
                       jump begin__action;
                   }
                   if (RunLife(actor,##Order)==0) L__M(##Order,1,actor);
               }
           }
           jump turn__end;
       }

       !  So now we must have an ordinary parser-generated action, unless
       !  inp1 is a "multiple object", in which case we: (a) check the
       !  multiple list isn't empty; (b) warn the player if it has been
       !  cut short because of excessive size, and (c) generate a sequence
       !  of actions from the list (stopping on death or movement away).

       if (i==0 || inp1~=0) Process();
       else
       {   multiflag=1;
           j=multiple_object-->0;
           if (j==0) { L__M(##Miscellany,2); jump late__error; }
           if (toomany_flag==1)
           {   toomany_flag=0; L__M(##Miscellany,1); }
           i=location;
           for (k=1:k<=j:k++)
           {   if (deadflag~=0) break;
               if (location~=i)
               {   print "(Since something dramatic has happened, \
                          your list of commands has been cut short.)^";
                   break;
               }
               l=multiple_object-->k; ResetVagueWords(l);
               PrintShortName(l); print ": ";
               inp1=l; noun=l; Process(); inp1=0; noun=0;
           }
       }

       .turn__end;

       !  No time passes if either (i) the verb was meta, or
       !  (ii) we've only had the implicit take before the "real"
       !  action to follow.

       if (notheld_mode==1) meta=1;
       if (deadflag==0 && meta==0) EndTurnSequence();
   }

   if (deadflag~=2) AfterLife();
   if (deadflag==0) jump late__error;

   print "^^    ";
   #IFV5; style bold; #ENDIF;
   print "***";
   if (deadflag==1) L__M(##Miscellany,3);
   if (deadflag==2) L__M(##Miscellany,4);
   if (deadflag>2)  { print " "; DeathMessage(); print " "; }
   print "***";
   #IFV5; style roman; #ENDIF;
   print "^^^";
   ScoreSub();
   DisplayStatus();

   .RRQPL;
   L__M(##Miscellany,5);
   .RRQL;
   print "> ";
   #IFV3; read buffer parse; #ENDIF;
   temp_global=0;
   #IFV5; read buffer parse DrawStatusLine; #ENDIF;
   i=parse-->1;
   if (i=='quit' or #n$q) quit;
   if (i=='restart')      @restart;
   if (i=='restore')      { RestoreSub(); jump RRQPL; }
   if (i=='fullscore' or 'full' && TASKS_PROVIDED==0)
   {   new_line; FullScoreSub(); jump RRQPL; }
   if (deadflag==2 && i=='amusing' && AMUSING_PROVIDED==0)
   {   new_line; Amusing(); jump RRQPL; }
#IFV5;
   if (i=='undo')
   {   if (undo_flag==0)
       {   L__M(##Miscellany,6);
           jump RRQPL;
       }
       if (undo_flag==1) jump UndoFailed2;
       @restore_undo i;
       if (i==0)
       {   .UndoFailed2; L__M(##Miscellany,7);
       }
       jump RRQPL;
   }
#ENDIF;
   L__M(##Miscellany,8);
   jump RRQL;
];

[ Process;
#IFDEF DEBUG;
   if (debug_flag & 2 ~= 0) TraceAction(0);
#ENDIF;
   if (meta==1 || BeforeRoutines()==0)
       indirect(#actions_table-->action);
];

#IFDEF DEBUG;
Array debug_anames table
[;##Inv "Inv";
  ##InvTall "InvTall";
  ##InvWide "InvWide";
  ##Take "Take";
  ##Drop "Drop";
  ##Remove "Remove";
  ##PutOn "PutOn";
  ##Insert "Insert";
  ##Transfer "Transfer";
  ##Empty "Empty";
  ##Enter "Enter";
  ##Exit "Exit";
  ##GetOff "GetOff";
  ##Go "Go";
  ##GoIn "GoIn";
  ##Look "Look";
  ##Examine "Examine";
  ##Search "Search";
  ##Give "Give";
  ##Show "Show";
  ##Unlock "Unlock";
  ##Lock "Lock";
  ##SwitchOn "SwitchOn";
  ##SwitchOff "SwitchOff";
  ##Open "Open";
  ##Close "Close";
  ##Disrobe "Disrobe";
  ##Wear "Wear";
  ##Eat "Eat";
  ##Yes "Yes";
  ##No "No";
  ##Burn "Burn";
  ##Pray "Pray";
  ##Wake "Wake";
  ##WakeOther "WakeOther";
  ##Consult "Consult";
  ##Kiss "Kiss";
  ##Think "Think";
  ##Smell "Smell";
  ##Listen "Listen";
  ##Taste "Taste";
  ##Touch "Touch";
  ##Dig "Dig";
  ##Cut "Cut";
  ##Jump "Jump";
  ##JumpOver "JumpOver";
  ##Tie "Tie";
  ##Drink "Drink";
  ##Fill "Fill";
  ##Sorry "Sorry";
  ##Strong "Strong";
  ##Mild "Mild";
  ##Attack "Attack";
  ##Swim "Swim";
  ##Swing "Swing";
  ##Blow "Blow";
  ##Rub "Rub";
  ##Set "Set";
  ##SetTo "SetTo";
  ##WaveHands "WaveHands";
  ##Wave "Wave";
  ##Pull "Pull";
  ##Push "Push";
  ##PushDir "PushDir";
  ##Turn "Turn";
  ##Squeeze "Squeeze";
  ##LookUnder "LookUnder";
  ##ThrowAt "ThrowAt";
  ##Answer "Answer";
  ##Buy "Buy";
  ##Ask "Ask";
  ##Tell "Tell";
  ##AskFor "AskFor";
  ##Sing "Sing";
  ##Climb "Climb";
  ##Wait "Wait";
  ##Sleep "Sleep";
  ##Order "Order";
  ##NotUnderstood "NotUnderstood";
];
[ DebugParameter w x n l;
  x=0-->4; x=x+(x->0)+1; l=x->0; n=(x+1)-->0; x=w-(x+3);
  print w;
  if (w>=1 && w<=top_object) print " (", (name) w, ")";
  if (x%l==0 && (x/l)<n) print " ('", (address) w, "')";
];
[ DebugAction a i;
  for (i=1:i<=debug_anames-->0:i=i+2)
  {   if (debug_anames-->i==a)
      {   print (string) debug_anames-->(i+1); rfalse; }
  }
  print a;
];
[ TraceAction source ar;
  if (source<2) { print "[Action "; DebugAction(action); }
  else
  {   if (ar==##Order)
      {   print "[Order to "; PrintShortName(actor); print ": ";
          DebugAction(action);
      }
      else
      {   print "[Life rule "; DebugAction(ar); }
  }
  if (noun~=0)   { print " with noun "; DebugParameter(noun);  }
  if (second~=0) { print " and second "; DebugParameter(second); }
  if (source==0) print " (from parser)";
  if (source==1) print " (from outside)";
  print "]^";
];
#ENDIF;

[ TestScope obj act a al sr x y;
  x=parser_one; y=parser_two;
  parser_one=obj; parser_two=0; a=actor; al=actors_location;
  sr=scope_reason; scope_reason=TESTSCOPE_REASON;
  if (act==0) actor=player; else actor=act;
  actors_location=actor;
  while (parent(actors_location)~=0)
      actors_location=parent(actors_location);
  SearchScope(location,player,0); scope_reason=sr; actor=a;
  actors_location=al; parser_one=x; x=parser_two; parser_two=y;
  return x;
];

[ LoopOverScope routine act x y a al;
  x = parser_one; y=scope_reason; a=actor; al=actors_location;
  parser_one=routine; if (act==0) actor=player; else actor=act;
  actors_location=actor;
  while (parent(actors_location)~=0)
      actors_location=parent(actors_location);
  scope_reason=LOOPOVERSCOPE_REASON;
  SearchScope(actors_location,actor,0);
  parser_one=x; scope_reason=y; actor=a; actors_location=al;
];

[ BeforeRoutines;
  if (GamePreRoutine()~=0) rtrue;
  if (RunRoutines(player,orders)~=0) rtrue;
  if (location~=0 && RunRoutines(location,before)~=0) rtrue;
  scope_reason=REACT_BEFORE_REASON; parser_one=0;
  SearchScope(location,player,0); scope_reason=PARSING_REASON;
  if (parser_one~=0) rtrue;
  if (inp1>1 && RunRoutines(inp1,before)~=0) rtrue;
  rfalse;
];

[ AfterRoutines;
  scope_reason=REACT_AFTER_REASON; parser_one=0;
  SearchScope(location,player,0); scope_reason=PARSING_REASON;
  if (parser_one~=0) rtrue;
  if (location~=0 && RunRoutines(location,after)~=0) rtrue;
  if (inp1>1 && RunRoutines(inp1,after)~=0) rtrue;
  return GamePostRoutine();
];

[ R_Process acti i j sn ss sa sse;
   sn=inp1; ss=inp2; sa=action; sse=self;
   inp1 = i; inp2 = j; noun=i; second=j; action=acti;

#IFDEF DEBUG;
   if (debug_flag & 2 ~= 0) TraceAction(1);
#ENDIF;

   if ((meta==1 || BeforeRoutines()==0) && action<256)
   {   indirect(#actions_table-->action);
       self=sse; inp1=sn; noun=sn; inp2=ss; second=ss; action=sa; rfalse;
   }
   self=sse; inp1=sn; noun=sn; inp2=ss; second=ss; action=sa; rtrue;
];

[ RunLife a j;
#IFDEF DEBUG;
   if (debug_flag & 2 ~= 0) TraceAction(2, j);
#ENDIF;
   reason_code = j; return RunRoutines(a,life);
];

[ LowKey_Menu menu_choices EntryR ChoiceR lines main_title i j;
  menu_nesting++;
 .LKRD;
  menu_item=0;
  lines=indirect(EntryR);
  main_title=item_name;

  print "--- "; print (string) main_title; print " ---^^";
  if (ZRegion(menu_choices)==3) print (string) menu_choices;
  else indirect(menu_choices);

 .LKML;
  print "^Type a number from 1 to ", lines,
        ", 0 to redisplay or press ENTER.^> ";

   #IFV3; read buffer parse; #ENDIF;
   temp_global=0;
   #IFV5; read buffer parse DrawStatusLine; #ENDIF;
   i=parse-->1;
   if (i=='quit' or #n$q || parse->1==0)
   {   menu_nesting--; if (menu_nesting>0) rfalse;
       if (deadflag==0) <<Look>>;
       rfalse;
   }
   i=TryNumber(1);
   if (i==0) jump LKRD;
   if (i<1 || i>lines) jump LKML;
   menu_item=i;
   j=indirect(ChoiceR);
   if (j==2) jump LKRD;
   if (j==3) rfalse;
   jump LKML;
];

#IFV3;
[ DoMenu menu_choices EntryR ChoiceR;
  LowKey_Menu(menu_choices,EntryR,ChoiceR);
];
#ENDIF;

#IFV5;
[ DoMenu menu_choices EntryR ChoiceR
         lines main_title main_wid cl i j oldcl pkey;
  if (pretty_flag==0)
  {   LowKey_Menu(menu_choices,EntryR,ChoiceR);
      rfalse;
  }
  menu_nesting++;
  menu_item=0;
  lines=indirect(EntryR);
  main_title=item_name; main_wid=item_width;
  cl=7;
  .ReDisplay;
      oldcl=0;
      @erase_window $ffff;
      i=lines+7;
      @split_window i;
      i = 0->33;
      if (i==0) i=80;
      @set_window 1;
      @set_cursor 1 1;
      style reverse;
      spaces(i); j=i/2-main_wid;
      @set_cursor 1 j;
      print (string) main_title;
      @set_cursor 2 1; spaces(i);
      @set_cursor 2 2; print "N = next subject";
      j=i-12; @set_cursor 2 j; print "P = previous";
      @set_cursor 3 1; spaces(i);
      @set_cursor 3 2; print "RETURN = read subject";
      j=i-17; @set_cursor 3 j;
      if (menu_nesting==1)
          print "  Q = resume game";
      else
          print "Q = previous menu";
      style roman;
      @set_cursor 5 2; font off;

      if (ZRegion(menu_choices)==3) print (string) menu_choices;
      else indirect(menu_choices);

      .KeyLoop;
      if (cl~=oldcl)
      {   if (oldcl>0) { @set_cursor oldcl 4; print " "; }
          @set_cursor cl 4; print "^]";
      }
      oldcl=cl;
      @read_char 1 0 0 pkey;
      if (pkey=='N' or 'n' or 130)
          { cl++; if (cl==7+lines) cl=7; jump KeyLoop; }
      if (pkey=='P' or 'p' or 129)
          { cl--; if (cl==6)  cl=6+lines; jump KeyLoop; }
      if ((pkey=='Q' or 'q' or 27)||(pkey==131)) { jump QuitHelp; }
      if (pkey==10 or 13 or 132)
      {   @set_window 0; font on;
          new_line; new_line; new_line;

          menu_item=cl-6;
          indirect(EntryR);

          @erase_window $ffff;
          @split_window 1;
          i = 0->33; if (i==0) { i=80; }
          @set_window 1; @set_cursor 1 1; style reverse; spaces(i);
          j=i/2-item_width;
          @set_cursor 1 j;
          print (string) item_name;
          style roman; @set_window 0; new_line;

          i = indirect(ChoiceR);
          if (i==2) jump ReDisplay;
          if (i==3) jump QuitHelp;

          print "^[Please press SPACE.]^";
          @read_char 1 0 0 pkey; jump ReDisplay;
      }
      jump KeyLoop;
      .QuitHelp;
      menu_nesting--; if (menu_nesting>0) rfalse;
      font on; @set_cursor 1 1;
      @erase_window $ffff; @set_window 0;
      new_line; new_line; new_line;
      if (deadflag==0) <<Look>>;
];  
#ENDIF;

[ StartTimer obj timer i;
   for (i=0:i<active_timers:i++)
       if (the_timers-->i==obj)
       {   if (timer_flags->i==2) RunTimeError(6,obj);
           rfalse;
       }
   for (i=0:i<active_timers:i++)
       if (the_timers-->i==0) jump FoundTSlot;
   i=active_timers++;
   if (i*2>=MAX_TIMERS) RunTimeError(4);
   .FoundTSlot;
   if (obj.&time_left==0) RunTimeError(5,obj);
   the_timers-->i=obj; timer_flags->i=1; obj.time_left=timer;
];

[ StopTimer obj i;
   for (i=0:i<active_timers:i++)
       if (the_timers-->i==obj) jump FoundTSlot2;
   rfalse;
   .FoundTSlot2;
   if (obj.&time_left==0) RunTimeError(5,obj);
   the_timers-->i=0; obj.time_left=0;
];

[ StartDaemon obj i;
   for (i=0:i<active_timers:i++)
       if (the_timers-->i==obj)
       {   if (timer_flags->i==1) RunTimeError(6,obj);
           rfalse;
       }
   for (i=0:i<active_timers:i++)
       if (the_timers-->i==0) jump FoundTSlot3;
   i=active_timers++;
   if (i*2>=MAX_TIMERS) RunTimeError(4);
   .FoundTSlot3;
   the_timers-->i=obj; timer_flags->i=2;
];

[ StopDaemon obj i;
   for (i=0:i<active_timers:i++)
       if (the_timers-->i==obj) jump FoundTSlot4;
   rfalse;
   .FoundTSlot4;
   the_timers-->i=0;
];

[ EndTurnSequence i j;

   turns++;
   if (the_time~=NULL)
   {   if (time_rate>=0) the_time=the_time+time_rate;
       else
       {   time_step--;
           if (time_step==0)
           {   the_time++;
               time_step = -time_rate;
           }
       }
       the_time=the_time % 1440;
   }
#IFDEF DEBUG;
   if (debug_flag & 4 ~= 0)
   {   for (i=0: i<active_timers: i++)
       {   j=the_timers-->i;
           if (j~=0)
           {   PrintShortName(j);
               if (timer_flags->i==2) print ": daemon";
               else
               { print ": timer with ", j.time_left, " turns to go"; }
               new_line;
           }
       }
   }
#ENDIF;
   for (i=0: deadflag==0 && i<active_timers: i++)
   {   j=the_timers-->i;
       if (j~=0)
       {   if (timer_flags->i==2) RunRoutines(j,daemon);
           else
           {   if (j.time_left==0)
               {   StopTimer(j);
                   RunRoutines(j,time_out);
               }
               else
                   j.time_left=j.time_left-1;
           }
       }
   }
   if (deadflag==0)
   {   scope_reason=EACH_TURN_REASON; verb_word=0;
       DoScopeAction(location); SearchScope(location,player,0);
       scope_reason=PARSING_REASON;
   }
   if (deadflag==0) TimePasses();
   if (deadflag==0)
   {   AdjustLight();
       objectloop (i in player)
           if (i hasnt moved)
           {   give i moved;
               if (i has scored)
               {   score=score+OBJECT_SCORE;
                   things_score=things_score+OBJECT_SCORE;
               }
           }
   }
];

[ AdjustLight flag i;
   i=lightflag;
   lightflag=OffersLight(parent(player));

   if (i==0 && lightflag==1)
   {   location=real_location; if (flag==0) <Look>;
   }

   if (i==1 && lightflag==0)
   {   real_location=location; location=thedark;
       if (flag==0) { NoteArrival();
                      return L__M(##Miscellany, 9); }
   }
   if (i==0 && lightflag==0) location=thedark;
];

[ OffersLight i j;
   if (i==0) rfalse;
   if (i has light) rtrue;
   objectloop (j in i)
       if (HasLightSource(j)==1) rtrue;
   if (i has enterable || IsSeeThrough(i)==1)
       return OffersLight(parent(i));
   rfalse;
];

[ HasLightSource i j ad;
   if (i==0) rfalse;
   if (i has light) rtrue;
   if (i has enterable || IsSeeThrough(i)==1)
   {   objectloop (i in i)
           if (HasLightSource(i)==1) rtrue;
   }
   ad = i.&add_to_scope;
   if (parent(i)~=0 && ad ~= 0)
   {   if (ad-->0 > top_object)
       {   ats_hls = 0; ats_flag = 1;
           RunRoutines(i, add_to_scope);
           ats_flag = 0; if (ats_hls == 1) rtrue;
       }
       else
       {   for (j=0:(2*j)<i.#add_to_scope:j++)
               if (HasLightSource(ad-->j)==1) rtrue;
       }
   }
   rfalse;
];

[ SayProS x;
  if (x==0) print "is unset";
  else { print "means "; DefArt(x); }
];

[ PronounsSub;
  print "At the moment, ~it~ "; SayProS(itobj);
  print ", ~him~ "; SayProS(himobj);
  if (player==selfobj) print " and"; else print ",";
  print " ~her~ "; SayProS(herobj);
  if (player==selfobj) ".";
  print " and ~me~ means "; @print_obj player; ".";
];

[ ChangePlayer obj flag i;
  if (obj.&number==0) return RunTimeError(7,obj);
  if (actor==player) actor=obj;
  give player ~transparent ~concealed;
  i=obj; while(parent(i)~=0) { if (i has animate) give i transparent;
                               i=parent(i); }
  if (player==selfobj) player.short_name="your former self";
  player.number=real_location; player=obj;
  if (player==selfobj) player.short_name=NULL;
  give player transparent concealed animate proper;
  i=player; while(parent(i)~=0) i=parent(i); location=i;
  real_location=player.number;
  if (real_location==0) real_location=location;
  lightflag=OffersLight(parent(player));
  if (lightflag==0) location=thedark;
  print_player_flag=flag;
];

[ ChangeDefault prop val;
   (0-->5)-->(prop-1) = val;
];

[ RandomEntry tab;
  if (tab-->0==0) return RunTimeError(8);
  return tab-->(random(tab-->0));
];

[ Indefart o;
   if (o hasnt proper) { PrintOrRun(o,article,1); print " "; }
   PrintShortName(o);
];

[ Defart o;
   if (o hasnt proper) print "the "; PrintShortName(o);
];

[ CDefart o;
   if (o hasnt proper) print "The "; PrintShortName(o);
];

[ PrintShortName o;
   if (o==0) { print "nothing"; rtrue; }
   if (o>top_object || o<0)
   {   switch(ZRegion(o))
       {   2: print "<routine ", o, ">"; rtrue;
           3: print "<string ~", (string) o, "~>"; rtrue;
       }
       print "<illegal object number ", o, ">"; rtrue;
   }
   if (o==player) { print "yourself"; rtrue; }
   if (o.&short_name~=0 && PrintOrRun(o,short_name,1)~=0) rtrue;
   @print_obj o;
];

! Provided for, e.g.,  print (DirectionName) obj.door_dir;

[ DirectionName d;
   switch(d)
   {   n_to: print "north";
       s_to: print "south";
       e_to: print "east";
       w_to: print "west";
       ne_to: print "northeast";
       nw_to: print "northwest";
       se_to: print "southeast";
       sw_to: print "southwest";
       u_to: print "up";
       d_to: print "down";
       in_to: print "in";
       out_to: print "out";
       default: return RunTimeError(9,d);
   }
];

[ Banner i;
   if (Story ~= 0)
   {
#IFV5; style bold; #ENDIF;
   print (string) Story;
#IFV5; style roman; #ENDIF;
   }
   if (Headline ~= 0)
       print (string) Headline;
   print "Release ", (0-->1) & $03ff, " / Serial number ";
   for (i=18:i<24:i++) print (char) 0->i;
   print " / Inform v"; inversion;
   print " Library ", (string) LibRelease;
#ifdef DEBUG;
   print " D";
#endif;
   new_line;
   if (standard_interpreter > 0)
       print "Standard interpreter ",
           standard_interpreter/256, ".", standard_interpreter%256, "^";
];

[ VersionSub;
  Banner();
#IFV5;
  print "Interpreter ", 0->$1e, " Version ", (char) 0->$1f, " / ";
#ENDIF;
  print "Library serial number ", (string) LibSerial, "^";
];

[ RunTimeError n p1 p2;
#IFDEF DEBUG;
  print "** Library error ", n, " (", p1, ",", p2, ") **^** ";
  switch(n)
  {   1: print "Adjective not found (this should not occur)";
      2: print "Property value not routine or string: ~",
               (NameTheProperty) p2, "~ of ~", (name) p1, "~ (", p1, ")";
      3: print "Entry in property list not routine or string: ~",
               (NameTheProperty) p2, "~ list of ~", (name) p1,
               "~ (", p1, ")";
      4: print "Too many timers/daemons are active simultaneously.  The \
                limit is the library constant MAX_TIMERS (currently ",
                MAX_TIMERS, ") and should be increased";
      5: print "Object ~", (name) p1, "~ has no ~time_left~ property";
      6: print "The object ~", (name) p1, "~ cannot be active as a \
                daemon and as a timer at the same time";
      7: print "The object ~", (name) p1, "~ can only be used as a player \
                object if it has the ~number~ property";
      8: print "Attempt to take random entry from an empty table array";
      9: print p1, " is not a valid direction property number";
      10: print "The player-object is outside the object tree";
      11: print "The room ~", (name) p1, "~ has no ~description~ property";
      default: print "(unexplained)";
  }
  " **";
#IFNOT;
  print_ret "** Library error ", n, " (", p1, ",", p2, ") **";
#ENDIF;
];

! ----------------------------------------------------------------------------
